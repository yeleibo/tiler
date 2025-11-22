module tiler

go 1.15

require (
	github.com/antonfisher/nested-logrus-formatter v1.3.0
	github.com/mattn/go-runewidth v0.0.9 // indirect
	github.com/paulmach/orb v0.1.6
	github.com/shiena/ansicolor v0.0.0-20230509054315-a9deabde6e02
	github.com/sirupsen/logrus v1.6.0
	github.com/spf13/viper v1.7.1
	github.com/teris-io/shortid v0.0.0-20220617161101-71ec9f2aa569
	gopkg.in/cheggaaa/pb.v1 v1.0.28
	modernc.org/sqlite v1.29.1
)

replace github.com/paulmach/orb v0.1.6 => github.com/atlasdatatech/orb v0.2.2
