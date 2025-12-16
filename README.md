
# 地图下载器 Tiler - map tiles downloader

A well-polished tile downloader

一个极速地图下载框架，支持谷歌、百度、高德、天地图、Mapbox、OSM、四维、易图通等。

- 支持多任务多线程配置，可任意设置

- 支持不同层级设置不同下载范围，以加速下载

- 支持轮廓精准下载，支持轮廓裁剪

- 支持矢量瓦片数据下载

- 支持文件和MBTILES两种存储方式

- 支持自定义瓦片地址
## 打包
 go build -ldflags="-s -w" -o tiler.exe
## 使用方式

1. 下载源代码在对应的平台上自己编译

2. 直接release发布页面, 下载对应平台的预编译程序

参照配置文件中的示例url更改为想要下载的地图地址，即可启动下载任务~
> 例如: url = "http://mt0.google.com/vt/lyrs=s&x={x}&y={y}&z={z}" ,地址中瓦片的xyz使用{x}{y}{z}代替，其他保持不变。

## 谷歌地图说明
- 影像层
  谷歌影像，分有偏移和无偏移两种，下载国内有偏移的影像需要在连接中加地区字段，如下为大陆地区偏移影像
  > url = "http://mt0.google.com/vt/lyrs=s&gl=CN&x={x}&y={y}&z={z}"
- 标注层
  影像标注，中文标注只有火星坐标，谷歌并不提供无偏移标注图层，所以通常只能下载有偏移的标注层，如下为大陆地区偏移标注
  > url = "http://mt0.google.com/vt/lyrs=h&gl=CN&x={x}&y={y}&z={z}"
- 使用
  在实际的使用中，要么保持系统的无偏移（这个时候需要校准有偏移的标注层），要么保持影像和标注的都有偏移，使用火星算法处理自己的数据

#### 谷歌图层类型lyrs=
- h 街道图，透明街道+标注
- m 街道图
- p 街道图
- r 街道图
- s 影像无标注
- t 地形图
- y 影像含标注


## 天地图说明
- 天地图影像,img_w
  > url = "https://t0.tianditu.gov.cn/DataServer?T=img_w&x={x}&y={y}&l={z}&tk=75f0434f240669f4a2df6359275146d2"
- 影像标注层,cia_w
  > url = "https://t0.tianditu.gov.cn/DataServer?T=cia_w&x={x}&y={y}&l={z}&tk=75f0434f240669f4a2df6359275146d2"

- 天地图矢量(地形图),vec_w
  > url = "https://t0.tianditu.gov.cn/DataServer?T=vec_w&x={x}&y={y}&l={z}&tk=75f0434f240669f4a2df6359275146d2"
- 矢量标注层,cva_w
  > url = "https://t0.tianditu.gov.cn/DataServer?T=cva_w&x={x}&y={y}&l={z}&tk=75f0434f240669f4a2df6359275146d2"

> 工具已经处理了天地图429限制，请合理使用！！！

## 获取geojson
### 阿里
https://datav.aliyun.com/portal/school/atlas/area_selector
### OpenStreetMap
https://nominatim.openstreetmap.org/search?q=%E6%AD%A6%E6%B1%89%E6%B1%9F%E6%B1%89%E5%8C%BA%E5%89%8D%E8%BF%9B%E8%A1%97%E9%81%93&polygon_geojson=1&format=jsonv2

把里面的q换成具体的关键字，结果检查"display_name":"前进街道, 江汉区, 武汉市, 湖北省, 中国" 看是否是自己需要的，然后将数据保存到geojson文件中

### 手动框选范围
https://geojson.io/#map=13.42/22.6365/113.6574