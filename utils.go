package main

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"sync"

	"github.com/paulmach/orb"
	"github.com/paulmach/orb/geojson"
	"github.com/paulmach/orb/maptile"
	"github.com/paulmach/orb/maptile/tilecover"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
)

// tileExistsInMBTile 检查瓦片是否已存在于 MBTiles 数据库中
func tileExistsInMBTile(tile maptile.Tile, db *sql.DB) bool {
	if db == nil {
		return false
	}
	var count int
	flipY := uint32(math.Pow(2.0, float64(tile.Z))) - 1 - tile.Y
	row := db.QueryRow("SELECT COUNT(*) FROM tiles WHERE zoom_level = ? AND tile_column = ? AND tile_row = ?", tile.Z, tile.X, flipY)
	err := row.Scan(&count)
	if err != nil {
		return false
	}
	return count > 0
}

// tileExistsInFiles 检查瓦片文件是否已存在于文件系统中
func tileExistsInFiles(tile maptile.Tile, task *Task) bool {
	fileName := filepath.Join(task.File, fmt.Sprintf(`%d`, tile.Z), fmt.Sprintf(`%d`, tile.X), fmt.Sprintf(`%d.%s`, tile.Y, task.TileMap.Format))
	_, err := os.Stat(fileName)
	return err == nil
}

func saveToMBTile(tile Tile, db *sql.DB) error {
	_, err := db.Exec("insert into tiles (zoom_level, tile_column, tile_row, tile_data) values (?, ?, ?, ?);", tile.T.Z, tile.T.X, tile.flipY(), tile.C)
	// _, err := db.Exec("insert or ignore into tiles (zoom_level, tile_column, tile_row, tile_data) values (?, ?, ?, ?);", tile.T.Z, tile.T.X, tile.flipY(), tile.C)
	if err != nil {
		return err
	}
	return nil
}

func saveToFiles(tile Tile, task *Task) error {
	dir := filepath.Join(task.File, fmt.Sprintf(`%d`, tile.T.Z), fmt.Sprintf(`%d`, tile.T.X))
	os.MkdirAll(dir, os.ModePerm)
	fileName := filepath.Join(dir, fmt.Sprintf(`%d.%s`, tile.T.Y, task.TileMap.Format))
	err := os.WriteFile(fileName, tile.C, os.ModePerm)
	if err != nil {
		return err
	}
	log.Println(fileName)
	return nil
}

func optimizeConnection(db *sql.DB) error {
	// _, err := db.Exec("PRAGMA synchronous=0")
	// if err != nil {
	// 	return err
	// }
	_, err := db.Exec("PRAGMA locking_mode=EXCLUSIVE")
	if err != nil {
		return err
	}
	_, err = db.Exec("PRAGMA journal_mode=DELETE")
	if err != nil {
		return err
	}
	return nil
}

func optimizeDatabase(db *sql.DB) error {
	_, err := db.Exec("ANALYZE;")
	if err != nil {
		return err
	}

	_, err = db.Exec("VACUUM;")
	if err != nil {
		return err
	}

	return nil
}

// setupProgressDB 初始化进度跟踪数据库
func setupProgressDB(task *Task) error {
	outdir := viper.GetString("output.directory")
	os.MkdirAll(outdir, os.ModePerm)
	progressFile := filepath.Join(outdir, fmt.Sprintf("%s.progress.db", task.Name))

	// 如果不启用恢复功能，删除旧的进度文件
	if !task.resume {
		os.Remove(progressFile)
	}

	log.Debugf("Opening progress database: %s", progressFile)
	db, err := sql.Open("sqlite", progressFile)
	if err != nil {
		return fmt.Errorf("failed to open progress database: %w", err)
	}

	// 确保数据库连接有效
	if err = db.Ping(); err != nil {
		return fmt.Errorf("failed to ping progress database: %w", err)
	}

	// 创建进度表
	_, err = db.Exec(`CREATE TABLE IF NOT EXISTS downloaded_tiles (
		z INTEGER NOT NULL,
		x INTEGER NOT NULL,
		y INTEGER NOT NULL,
		downloaded_at DATETIME DEFAULT CURRENT_TIMESTAMP,
		PRIMARY KEY (z, x, y)
	)`)
	if err != nil {
		return fmt.Errorf("failed to create downloaded_tiles table: %w", err)
	}
	log.Debug("Created downloaded_tiles table")

	// 创建索引加速查询
	_, err = db.Exec(`CREATE INDEX IF NOT EXISTS idx_tile ON downloaded_tiles(z, x, y)`)
	if err != nil {
		return fmt.Errorf("failed to create index: %w", err)
	}

	// 创建断点记录表，记录当前下载到哪个层级和位置
	_, err = db.Exec(`CREATE TABLE IF NOT EXISTS resume_point (
		id INTEGER PRIMARY KEY CHECK (id = 1),
		layer_index INTEGER NOT NULL,
		last_update DATETIME DEFAULT CURRENT_TIMESTAMP
	)`)
	if err != nil {
		return fmt.Errorf("failed to create resume_point table: %w", err)
	}
	log.Debug("Created resume_point table")

	task.progressDB = db
	return nil
}

// markTileAsDownloaded 标记瓦片为已下载
func markTileAsDownloaded(tile maptile.Tile, db *sql.DB) error {
	if db == nil {
		return nil
	}
	_, err := db.Exec("INSERT OR IGNORE INTO downloaded_tiles (z, x, y) VALUES (?, ?, ?)", tile.Z, tile.X, tile.Y)
	return err
}

// batchMarkTilesAsDownloaded 批量标记瓦片为已下载
func batchMarkTilesAsDownloaded(tiles []maptile.Tile, db *sql.DB) error {
	if db == nil || len(tiles) == 0 {
		return nil
	}

	// 开始事务
	tx, err := db.Begin()
	if err != nil {
		return err
	}
	defer tx.Rollback()

	// 准备批量插入语句
	stmt, err := tx.Prepare("INSERT OR IGNORE INTO downloaded_tiles (z, x, y) VALUES (?, ?, ?)")
	if err != nil {
		return err
	}
	defer stmt.Close()

	// 批量执行插入
	for _, tile := range tiles {
		_, err = stmt.Exec(tile.Z, tile.X, tile.Y)
		if err != nil {
			return err
		}
	}

	// 提交事务
	return tx.Commit()
}

// isTileDownloaded 检查瓦片是否已下载（从进度数据库）
func isTileDownloaded(tile maptile.Tile, db *sql.DB) bool {
	if db == nil {
		return false
	}
	var count int
	row := db.QueryRow("SELECT COUNT(*) FROM downloaded_tiles WHERE z = ? AND x = ? AND y = ?", tile.Z, tile.X, tile.Y)
	err := row.Scan(&count)
	if err != nil {
		return false
	}
	return count > 0
}

// saveResumePoint 保存当前下载到的层级索引
func saveResumePoint(layerIndex int, db *sql.DB) error {
	if db == nil {
		return nil
	}
	_, err := db.Exec("INSERT OR REPLACE INTO resume_point (id, layer_index, last_update) VALUES (1, ?, datetime('now'))", layerIndex)
	return err
}

// getResumePoint 获取上次中断时的层级索引，如果没有记录返回 -1
func getResumePoint(db *sql.DB) int {
	if db == nil {
		return -1
	}
	var layerIndex int
	row := db.QueryRow("SELECT layer_index FROM resume_point WHERE id = 1")
	err := row.Scan(&layerIndex)
	if err != nil {
		return -1 // 没有记录，从头开始
	}
	return layerIndex
}

// getDownloadedTilesForZoom 获取指定 zoom 级别已下载的所有瓦片，返回一个 set 用于快速查找
func getDownloadedTilesForZoom(zoom int, db *sql.DB) map[uint64]bool {
	result := make(map[uint64]bool)
	if db == nil {
		return result
	}

	rows, err := db.Query("SELECT x, y FROM downloaded_tiles WHERE z = ?", zoom)
	if err != nil {
		log.Warnf("Failed to query downloaded tiles for zoom %d: %v", zoom, err)
		return result
	}
	defer rows.Close()

	for rows.Next() {
		var x, y uint32
		if err := rows.Scan(&x, &y); err != nil {
			continue
		}
		// 用 x<<32 | y 作为唯一 key
		key := uint64(x)<<32 | uint64(y)
		result[key] = true
	}

	return result
}

// getDownloadedCountForZoom 获取指定 zoom 级别已下载的瓦片数量
func getDownloadedCountForZoom(zoom int, db *sql.DB) int64 {
	if db == nil {
		return 0
	}
	var count int64
	row := db.QueryRow("SELECT COUNT(*) FROM downloaded_tiles WHERE z = ?", zoom)
	err := row.Scan(&count)
	if err != nil {
		return 0
	}
	return count
}

func loadFeature(path string) *geojson.Feature {
	data, err := os.ReadFile(path)
	if err != nil {
		log.Fatalf("unable to read file: %v", err)
	}

	f, err := geojson.UnmarshalFeature(data)
	if err == nil {
		return f
	}

	fc, err := geojson.UnmarshalFeatureCollection(data)
	if err == nil {
		if len(fc.Features) != 1 {
			log.Fatalf("must have 1 feature: %v", len(fc.Features))
		}
		return fc.Features[0]
	}

	g, err := geojson.UnmarshalGeometry(data)
	if err != nil {
		log.Fatalf("unable to unmarshal feature: %v", err)
	}

	return geojson.NewFeature(g.Geometry())
}

func loadFeatureCollection(path string) *geojson.FeatureCollection {
	data, err := os.ReadFile(path)
	if err != nil {
		log.Fatalf("unable to read file: %v", err)
	}

	fc, err := geojson.UnmarshalFeatureCollection(data)
	if err != nil {
		log.Fatalf("unable to unmarshal feature: %v", err)
	}

	count := 0
	for i := range fc.Features {
		if fc.Features[i].Properties["name"] != "original" {
			fc.Features[count] = fc.Features[i]
			count++
		}
	}
	fc.Features = fc.Features[:count]

	return fc
}

func loadCollection(path string) orb.Collection {
	data, err := os.ReadFile(path)
	if err != nil {
		log.Fatalf("unable to read file: %v", err)
	}

	var collection orb.Collection

	// 尝试解析标准的 FeatureCollection
	fc, err := geojson.UnmarshalFeatureCollection(data)
	if err == nil {
		for _, f := range fc.Features {
			collection = append(collection, f.Geometry)
		}
		return collection
	}

	// 尝试解析 OpenStreetMap Nominatim 格式 (JSON 数组)
	var nominatimResults []struct {
		GeoJSON json.RawMessage `json:"geojson"`
	}
	err = json.Unmarshal(data, &nominatimResults)
	if err == nil && len(nominatimResults) > 0 {
		for _, result := range nominatimResults {
			if len(result.GeoJSON) > 0 {
				geom, err := geojson.UnmarshalGeometry(result.GeoJSON)
				if err == nil {
					collection = append(collection, geom.Geometry())
				}
			}
		}
		if len(collection) > 0 {
			return collection
		}
	}

	// 尝试解析单个 Geometry
	geom, err := geojson.UnmarshalGeometry(data)
	if err == nil {
		collection = append(collection, geom.Geometry())
		return collection
	}

	log.Fatalf("unable to parse GeoJSON file: %s, tried FeatureCollection, Nominatim array, and Geometry formats", path)
	return collection
}

// output gets called if there is a test failure for debugging.
func output(name string, r *geojson.FeatureCollection) {
	f := loadFeature("./data/" + name + ".geojson")
	if f.Properties == nil {
		f.Properties = make(geojson.Properties)
	}

	f.Properties["fill"] = "#FF0000"
	f.Properties["fill-opacity"] = "0.5"
	f.Properties["stroke"] = "#FF0000"
	f.Properties["name"] = "original"
	r.Append(f)

	data, err := json.MarshalIndent(r, "", " ")
	if err != nil {
		log.Fatalf("error marshalling json: %v", err)
	}

	err = os.WriteFile("failure_"+name+".geojson", data, 0644)
	if err != nil {
		log.Fatalf("write file failure: %v", err)
	}
}

// output gets called if there is a test failure for debugging.
func output2(name string, r *geojson.FeatureCollection, wg *sync.WaitGroup) {
	defer wg.Done()
	data, err := json.MarshalIndent(r, "", " ")
	if err != nil {
		log.Fatalf("error marshalling json: %v", err)
	}

	err = os.WriteFile(name+".geojson", data, 0644)
	if err != nil {
		log.Fatalf("write file failure: %v", err)
	}
}

func getZoomCount(g orb.Geometry, minz int, maxz int) map[int]int64 {

	info := make(map[int]int64)
	for z := minz; z <= maxz; z++ {
		info[z] = tilecover.GeometryCount(g, maptile.Zoom(z))
	}
	return info
}
