- render: metadata completed zoom levels DONE
- profile: zoomlevel depends on area DONE
- db: store gzipped tiles DONE
- auto clean database DONE

- db: compact tile metaformat

- coastline shapefile
- improve performance / parallel
- feature wraparound 180/-180
- iterate -> zoom level 0: 2tiles?!
- iterate -> coastline: south america missing?!

- profile: warning if layer ist not set?!
- render: fast bounding box check in deserialize (same as zoomlevel)

- optimize: merge nearby shapes with equal properties

- render: order shapes by area
- profile: respect Key:layer
- profile: road names
- profile: road oneway direction
- profile: air road
- profile: tram

- profile: improave landuse railway
- profile: power / substation
- profile: rail main/greyed out


- profile: poi shopping
- profile: poi public transport
- profile: poi traffic lights
- profile: location names
- profile: woog steeg = building
- profile: foreign names?!

- database: variant for metadata
- geometry: custom block allocator for geometry stuff
- render: parallelize
- import: parallelize
