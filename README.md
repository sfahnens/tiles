# tiles

All-in-one vector tile server backend.

Project Status: Basic functionality works, performance (import and server) is good (also for the planetfile), fancy features are in active development.

![Style Example](https://raw.githubusercontent.com/sfahnens/tiles/screenshot/screenshot.png)

## Features

* All-in-one backend solution
  * Standalone server for standard/background maps
  * Easily embeddable for application specific datasets
* Serves vector tiles https://github.com/mapbox/vector-tile-spec (consumable with any compatible rendering library e.g. https://github.com/mapbox/mapbox-gl-js)
* Read Openstreetmap geometry from standard .osm.pbf files
* Read Openstreetmap coastline data from shapfiles
* Lua scripting for map profiles

## Quickstart

On Ubuntu 20.04 LTS:

```
sudo apt update
sudo apt install git wget cmake ninja-build build-essential libboost1.71-all-dev

git clone git@github.com:sfahnens/tiles.git
cd tiles && mkdir build && cd build

cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
ninja tiles-import tiles-server

# download coastline data
wget https://osmdata.openstreetmap.de/download/land-polygons-complete-4326.zip

# download openstreetmap germany dataset
wget https://download.geofabrik.de/europe/germany-latest.osm.pbf

./tiles-import --osm_fname germany-latest.osm.pbf --coastlines_fname land-polygons-complete-4326.zip

./tiles-server

# Now, go to localhost:8888 in your browser.
```

## Performance

(Coming soon.)


## License

MIT
