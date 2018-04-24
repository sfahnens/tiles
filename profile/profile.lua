

function process_node(node)
  if node:has_tag("place", "city") then
    node:set_target_layer("cities")
    node:add_tag_as_metadata("name")
    node:set_approved_full()
  end
end

function process_way(way)
  -- if not way:has_tag("railway", "rail") or
  --    way:has_any_tag("usage", {"industrial", "military", "test", "tourism"}) or
  --    way:has_any_tag("service", {"yard", "spur"}) or
  --    way:has_tag("railway:preserved", "yes") then
  --   return
  -- end

  if way:has_any_tag2("highway") then
    way:set_target_layer("road")

    if way:has_tag("highway", "motorway") or
       way:has_tag("highway", "trunk") then
      way:set_approved_min(5)

    elseif way:has_tag("highway", "motorway_link") or
           way:has_tag("highway", "trunk_link") or
           way:has_tag("highway", "primary") or
           way:has_tag("highway", "secondary") or
           way:has_tag("highway", "tertiary") then
      way:set_approved_min(9)
    end

  elseif way:has_tag("railway", "rail") then
    if way:has_tag("usage", "industrial") or
       way:has_tag("usage", "military") or
       way:has_tag("usage", "test") or
       way:has_tag("usage", "tourism") or
       way:has_tag("service", "yard") or
       way:has_tag("service", "spur") or
       way:has_tag("railway:preserved", "yes") then
      return
    end

    way:set_target_layer("rail")
    way:set_approved_full()
  end
end


function process_area(area)
  -- if area:has_any_tag2("landuse",
  --   "residential", "retail", "industrial", "forest", "farmland", "commercial") then
  --   area:set_target_layer("landuse")
  --   area:add_tag_as_metadata("landuse")
  --   area:set_approved()
  --   return
  -- end

   if area:has_any_tag2("landuse", "residential") then
    area:set_target_layer("landuse")
    area:add_tag_as_metadata("landuse")
    area:set_approved_full()
    return
  end


  -- if not area:has_tag("building", "yes") or
  --    area:has_tag("building", "residential") then
  --   return
  -- end

  -- area:set_target_layer("building")
  -- area:set_approved()
end
