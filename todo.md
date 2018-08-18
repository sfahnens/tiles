== DONE:
- render: metadata completed zoom levels DONE
- profile: zoomlevel depends on area DONE
- db: store gzipped tiles DONE
- auto clean database DONE
- iterate -> zoom level 0: 2tiles?! DONE
- iterate -> coastline: south america missing?! DONE
- coastline shapefile base DONE
- coastline shapefile parallel DONE
- improve performance DONE
- render: fast bounding box check in deserialize (same as zoomlevel) DONE
- database disk usage statistics
- fix coastline on tiles with features
- compact feature storage: 4kb pages of lmdb lead to bad memory efficiency
- fix get_tile bottleneck: iteration/skip is slower than rendering (z>=11)

== HIGH PRIO:

== LOW PRIO:
- proper feature wraparound 180/-180 <-> artifacts on island?!
- prepare tiles with empty database?!

- profile: warning if layer ist not set?!

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

- prepare tiles: investigate: some tiles mising on lower z for full coastlines
- better live statistics (wait some time to aggregate multiple requests)

- skip simplify for simple features (polygon with 4 points -> binary)
- improve geometry clipping: compute unclipped z-range
- use clipper for geometry clipping
- implement custom arena allocator
- use https://github.com/ebiggers/libdeflate/blob/master/libdeflate.h for compression