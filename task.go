package main

import (
	"bytes"
	"compress/gzip"
	"context"
	"database/sql"
	"fmt"
	"io"
	"math"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/paulmach/orb"
	"github.com/paulmach/orb/maptile"
	"github.com/paulmach/orb/maptile/tilecover"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"github.com/teris-io/shortid"
	pb "gopkg.in/cheggaaa/pb.v1"
)

// MBTileVersion mbtiles版本号
const MBTileVersion = "1.2"

// Task 下载任务
type Task struct {
	ID                 string
	Name               string
	Description        string
	File               string
	Min                int
	Max                int
	Layers             []Layer
	TileMap            TileMap
	Total              int64
	Current            int64
	Bar                *pb.ProgressBar
	db                 *sql.DB
	progressDB         *sql.DB
	workerCount        int
	savePipeSize       int
	timeDelay          int
	bufSize            int
	tileWG             sync.WaitGroup
	abort, pause, play chan struct{}
	workers            chan maptile.Tile
	savingpipe         chan Tile
	tileSet            Set
	outformat          string
	skipExisting       bool
	resume             bool
	progressBuffer       []maptile.Tile
	progressMutex        sync.Mutex
	progressTicker       *time.Ticker
	progressDone         chan struct{}
	progressInterval     int
}

// NewTask 创建下载任务
func NewTask(layers []Layer, m TileMap) *Task {
	if len(layers) == 0 {
		return nil
	}
	id, _ := shortid.Generate()

	task := Task{
		ID:      id,
		Name:    m.Name,
		Layers:  layers,
		Min:     m.Min,
		Max:     m.Max,
		TileMap: m,
	}

	for i := 0; i < len(layers); i++ {
		if layers[i].URL == "" {
			layers[i].URL = m.URL
		}
		layers[i].Count = tilecover.CollectionCount(layers[i].Collection, maptile.Zoom(layers[i].Zoom))
		log.Printf("zoom: %d, tiles: %d \n", layers[i].Zoom, layers[i].Count)
		task.Total += layers[i].Count
	}
	task.abort = make(chan struct{})
	task.pause = make(chan struct{})
	task.play = make(chan struct{})

	task.workerCount = viper.GetInt("task.workers")
	task.savePipeSize = viper.GetInt("task.savepipe")
	task.timeDelay = viper.GetInt("task.timedelay")
	task.workers = make(chan maptile.Tile, task.workerCount)
	task.savingpipe = make(chan Tile, task.savePipeSize)
	task.bufSize = viper.GetInt("task.mergebuf")
	task.tileSet = Set{M: make(maptile.Set)}

	task.outformat = viper.GetString("output.format")
	task.skipExisting = viper.GetBool("task.skipexisting")
	task.resume = viper.GetBool("task.resume")
	task.progressInterval = viper.GetInt("task.progressinterval")

	// 设置默认值
	if task.progressInterval <= 0 {
		task.progressInterval = 10
	}

	return &task
}

// Bound 范围
func (task *Task) Bound() orb.Bound {
	bound := orb.Bound{Min: orb.Point{1, 1}, Max: orb.Point{-1, -1}}
	for _, layer := range task.Layers {
		for _, g := range layer.Collection {
			bound = bound.Union(g.Bound())
		}
	}
	return bound
}

// Center 中心点
func (task *Task) Center() orb.Point {
	layer := task.Layers[len(task.Layers)-1]
	bound := orb.Bound{Min: orb.Point{1, 1}, Max: orb.Point{-1, -1}}
	for _, g := range layer.Collection {
		bound = bound.Union(g.Bound())
	}
	return bound.Center()
}

// MetaItems 输出
func (task *Task) MetaItems() map[string]string {
	b := task.Bound()
	c := task.Center()
	data := map[string]string{
		"id":          task.ID,
		"name":        task.Name,
		"description": task.Description,
		"attribution": `<a href="http://www.atlasdata.cn/" target="_blank">&copy; MapCloud</a>`,
		"basename":    task.TileMap.Name,
		"format":      task.TileMap.Format,
		"type":        task.TileMap.Schema,
		"pixel_scale": strconv.Itoa(TileSize),
		"version":     MBTileVersion,
		"bounds":      fmt.Sprintf(`%f,%f,%f,%f`, b.Left(), b.Bottom(), b.Right(), b.Top()),
		"center":      fmt.Sprintf(`%f,%f,%d`, c.X(), c.Y(), (task.Min+task.Max)/2),
		"minzoom":     strconv.Itoa(task.Min),
		"maxzoom":     strconv.Itoa(task.Max),
		"json":        task.TileMap.JSON,
	}
	return data
}

// SetupMBTileTables 初始化配置MBTile库
func (task *Task) SetupMBTileTables() error {

	if task.File == "" {
		outdir := viper.GetString("output.directory")
		os.MkdirAll(outdir, os.ModePerm)
		task.File = filepath.Join(outdir, fmt.Sprintf("%s.mbtiles", task.Name))
	}
	// 如果启用跳过已存在瓦片，不删除现有数据库文件
	if !task.skipExisting {
		os.Remove(task.File)
	}
	db, err := sql.Open("sqlite", task.File)
	if err != nil {
		return err
	}

	err = optimizeConnection(db)
	if err != nil {
		return err
	}

	_, err = db.Exec("create table if not exists tiles (zoom_level integer, tile_column integer, tile_row integer, tile_data blob);")
	if err != nil {
		return err
	}

	_, err = db.Exec("create table if not exists metadata (name text, value text);")
	if err != nil {
		return err
	}

	_, err = db.Exec("create unique index name on metadata (name);")
	if err != nil {
		return err
	}

	_, err = db.Exec("create unique index tile_index on tiles(zoom_level, tile_column, tile_row);")
	if err != nil {
		return err
	}

	// Load metadata.
	for name, value := range task.MetaItems() {
		_, err := db.Exec("insert into metadata (name, value) values (?, ?)", name, value)
		if err != nil {
			return err
		}
	}

	task.db = db //保存任务的库连接
	return nil
}

func (task *Task) abortFun() {
	// os.Stdin.Read(make([]byte, 1)) // read a single byte
	// <-time.After(8 * time.Second)
	task.abort <- struct{}{}
}

func (task *Task) pauseFun() {
	// os.Stdin.Read(make([]byte, 1)) // read a single byte
	// <-time.After(3 * time.Second)
	task.pause <- struct{}{}
}

func (task *Task) playFun() {
	// os.Stdin.Read(make([]byte, 1)) // read a single byte
	// <-time.After(5 * time.Second)
	task.play <- struct{}{}
}

// SavePipe 保存瓦片管道
func (task *Task) savePipe() {
	for tile := range task.savingpipe {
		err := saveToMBTile(tile, task.db)
		if err != nil {
			if strings.HasPrefix(err.Error(), "UNIQUE constraint failed") {
				log.Warnf("save %v tile to mbtiles db error ~ %s", tile.T, err)
			} else {
				log.Errorf("save %v tile to mbtiles db error ~ %s", tile.T, err)
			}
		}
	}
}

// SaveTile 保存瓦片
func (task *Task) saveTile(tile Tile) error {
	// defer task.wg.Done()
	err := saveToFiles(tile, task)
	if err != nil {
		log.Errorf("create %v tile file error ~ %s", tile.T, err)
	}
	return nil
}

// tileFetcher 瓦片加载器
func (task *Task) tileFetcher(mt maptile.Tile, url string) {
	start := time.Now()
	defer task.tileWG.Done() //结束该瓦片请求
	defer func() {
		<-task.workers //workers完成并清退
	}()

	// 如果启用了任务恢复，检查进度数据库
	if task.resume && isTileDownloaded(mt, task.progressDB) {
		log.Debugf("tile(z:%d, x:%d, y:%d) already downloaded (from progress), skipping...", mt.Z, mt.X, mt.Y)
		return
	}

	// 如果启用了跳过已存在瓦片的功能，检查瓦片是否已存在
	if task.skipExisting {
		var exists bool
		if task.outformat == "mbtiles" {
			exists = tileExistsInMBTile(mt, task.db)
		} else {
			exists = tileExistsInFiles(mt, task)
		}
		if exists {
			log.Debugf("tile(z:%d, x:%d, y:%d) already exists, skipping...", mt.Z, mt.X, mt.Y)
			// 标记为已下载
			if task.resume {
				markTileAsDownloaded(mt, task.progressDB)
			}
			return
		}
	}

	prep := func(t maptile.Tile, url string) string {
		url = strings.Replace(url, "{x}", strconv.Itoa(int(t.X)), -1)
		url = strings.Replace(url, "{y}", strconv.Itoa(int(t.Y)), -1)
		maxY := int(math.Pow(2, float64(t.Z))) - 1
		url = strings.Replace(url, "{-y}", strconv.Itoa(maxY-int(t.Y)), -1)
		url = strings.Replace(url, "{z}", strconv.Itoa(int(t.Z)), -1)
		return url
	}
	tile := prep(mt, url)
	client := &http.Client{
		CheckRedirect: func(req *http.Request, via []*http.Request) error {
			// 自定义重定向的行为
			return http.ErrUseLastResponse // 使用最后一个响应
		},
	}
	req, err := http.NewRequest("GET", tile, nil)
	if err != nil {
		fmt.Println("创建请求失败:", err)
		return
	}

	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36")
	req.Header.Set("Referer", "https://map.tianditu.gov.cn")
	// req.Header.Set("Referer", "https://jiangsu.tianditu.gov.cn")
	resp, err := client.Do(req)
	if err != nil {
		fmt.Println("发送请求失败:", err)
		return
	}
	defer resp.Body.Close()
	// resp, err := http.Get(tile)
	// if err != nil {
	// 	log.Errorf("fetch :%s error, details: %s ~", tile, err)
	// 	return
	// }
	// defer resp.Body.Close()
	if resp.StatusCode != 200 {
		log.Errorf("fetch %v tile error, status code: %d ~", tile, resp.StatusCode)
		return
	}
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		log.Errorf("read %v tile error ~ %s", mt, err)
		return
	}
	if len(body) == 0 {
		log.Warnf("nil tile %v ~", mt)
		return //zero byte tiles n
	}
	// tiledata
	td := Tile{
		T: mt,
		C: body,
	}

	if task.TileMap.Format == PBF {
		var buf bytes.Buffer
		zw := gzip.NewWriter(&buf)
		_, err = zw.Write(body)
		if err != nil {
			log.Fatal(err)
		}
		if err := zw.Close(); err != nil {
			log.Fatal(err)
		}
		td.C = buf.Bytes()
	}

	//enable savingpipe
	if task.outformat == "mbtiles" {
		task.savingpipe <- td
	} else {
		// task.wg.Add(1)
		task.saveTile(td)
	}

	// 添加到进度缓冲区（批量写入）
	if task.resume {
		task.progressMutex.Lock()
		task.progressBuffer = append(task.progressBuffer, mt)
		task.progressMutex.Unlock()
	}

	cost := time.Since(start).Milliseconds()
	log.Infof("tile(z:%d, x:%d, y:%d), %dms , %.2f kb, %s ...\n", mt.Z, mt.X, mt.Y, cost, float32(len(body))/1024.0, tile)
}

// DownloadZoom 下载指定层级
func (task *Task) downloadLayer(layer Layer) {
	bar := pb.New64(layer.Count).Prefix(fmt.Sprintf("Zoom %d : ", layer.Zoom)).Postfix("\n")
	// bar.SetRefreshRate(time.Second)
	bar.Start()
	// bar.SetMaxWidth(300)

	var tilelist = make(chan maptile.Tile, task.bufSize)

	go tilecover.CollectionChannel(layer.Collection, maptile.Zoom(layer.Zoom), tilelist)

	for tile := range tilelist {
		// log.Infof(`fetching tile %v ~`, tile)
		select {
		case task.workers <- tile:
			//设置请求发送间隔时间
			time.Sleep(time.Duration(task.timeDelay) * time.Millisecond)
			bar.Increment()
			task.Bar.Increment()
			task.tileWG.Add(1)
			go task.tileFetcher(tile, layer.URL)
		case <-task.abort:
			log.Infof("Task %s got canceled.", task.ID)
			close(tilelist)
		case <-task.pause:
			log.Infof("Task %s suspended.", task.ID)
			select {
			case <-task.play:
				log.Infof("Task %s go on.", task.ID)
			case <-task.abort:
				log.Infof("Task %s got canceled.", task.ID)
				close(tilelist)
			}
		}
	}
	//等待该层结束
	task.tileWG.Wait()
	bar.FinishPrint(fmt.Sprintf("Task %s Zoom %d finished ~", task.ID, layer.Zoom))
}

// flushProgress 刷新进度缓冲区到数据库
func (task *Task) flushProgress() {
	task.progressMutex.Lock()
	if len(task.progressBuffer) == 0 {
		task.progressMutex.Unlock()
		return
	}
	// 复制缓冲区并清空
	tiles := make([]maptile.Tile, len(task.progressBuffer))
	copy(tiles, task.progressBuffer)
	task.progressBuffer = task.progressBuffer[:0]
	task.progressMutex.Unlock()

	// 批量写入数据库
	err := batchMarkTilesAsDownloaded(tiles, task.progressDB)
	if err != nil {
		log.Warnf("failed to batch save progress (%d tiles): %v", len(tiles), err)
	} else {
		log.Debugf("progress saved: %d tiles", len(tiles))
	}
}

// startProgressFlusher 启动进度刷新定时器
func (task *Task) startProgressFlusher() {
	task.progressDone = make(chan struct{})
	task.progressTicker = time.NewTicker(time.Duration(task.progressInterval) * time.Minute)

	go func() {
		for {
			select {
			case <-task.progressTicker.C:
				task.flushProgress()
			case <-task.progressDone:
				task.progressTicker.Stop()
				return
			}
		}
	}()
}

// stopProgressFlusher 停止进度刷新并保存剩余进度
func (task *Task) stopProgressFlusher() {
	if task.progressDone != nil {
		close(task.progressDone)
	}
	// 最后刷新一次，保存所有剩余进度
	task.flushProgress()
}

// Download 开启下载任务
func (task *Task) Download() {
	//g orb.Geometry, minz int, maxz int
	task.Bar = pb.New64(task.Total).Prefix("Task : ").Postfix("\n")
	// task.Bar.SetRefreshRate(10 * time.Second)
	// task.Bar.Format("<.- >")
	task.Bar.Start()

	// 初始化进度数据库
	startLayerIndex := 0
	if task.resume {
		err := setupProgressDB(task)
		if err != nil {
			log.Warnf("Failed to setup progress database: %v, continuing without resume support", err)
			task.resume = false
		} else {
			defer task.progressDB.Close()
			defer task.stopProgressFlusher() // 确保退出时保存所有进度
			task.progressBuffer = make([]maptile.Tile, 0, 10000)
			task.startProgressFlusher() // 启动定时刷新

			// 获取上次中断的层级索引
			startLayerIndex = getResumePoint(task.progressDB)
			if startLayerIndex > 0 {
				log.Infof("Resuming from layer index %d (zoom %d)", startLayerIndex, task.Layers[startLayerIndex].Zoom)
				// 计算已跳过的瓦片数，更新进度条
				var skippedCount int64
				for i := 0; i < startLayerIndex; i++ {
					skippedCount += task.Layers[i].Count
				}
				task.Bar.Add64(skippedCount)
			} else {
				log.Info("Starting fresh download")
			}
			log.Info("Progress tracking enabled, task can be resumed after interruption")
			log.Infof("Progress will be saved every %d minutes", task.progressInterval)
		}
	}

	if task.outformat == "mbtiles" {
		task.SetupMBTileTables()
	} else {
		if task.File == "" {
			outdir := viper.GetString("output.directory")
			task.File = filepath.Join(outdir, task.Name)
		}
		os.MkdirAll(task.File, os.ModePerm)
	}
	go task.savePipe()

	// 从断点位置开始下载
	for i := startLayerIndex; i < len(task.Layers); i++ {
		// 保存当前层级索引
		if task.resume {
			saveResumePoint(i, task.progressDB)
		}
		task.downloadLayer(task.Layers[i])
	}

	// 下载完成，清除断点记录
	if task.resume {
		saveResumePoint(-1, task.progressDB) // -1 表示已完成
	}

	task.Bar.FinishPrint(fmt.Sprintf("Task %s finished ~", task.ID))
}

// SaveProgressOnExit 在程序退出时保存进度（供信号处理使用）
func (task *Task) SaveProgressOnExit() {
	if task.resume && task.progressDB != nil {
		log.Info("Flushing progress buffer before exit...")
		task.flushProgress()
		log.Info("Progress saved successfully")
	}
}

// DownloadWithContext 开启下载任务（支持上下文取消）
func (task *Task) DownloadWithContext(ctx context.Context) {
	task.Download()
}
