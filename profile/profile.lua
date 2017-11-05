

function process_node(node)
  if node:has_tag("place", "city") then
    node:set_target_layer("cities")
    node:add_tag_as_metadata("name")
    node:set_approved()
  end
end

function process_way(way)
  -- if not way:has_tag("railway", "rail") or
  --    way:has_any_tag("usage", {"industrial", "military", "test", "tourism"}) or
  --    way:has_any_tag("service", {"yard", "spur"}) or
  --    way:has_tag("railway:preserved", "yes") then
  --   return
  -- end
  
  if not way:has_tag("railway", "rail") or
     way:has_tag("usage", "industrial") or
     way:has_tag("usage", "military") or
     way:has_tag("usage", "test") or
     way:has_tag("usage", "tourism") or
     way:has_tag("service", "yard") or
     way:has_tag("service", "spur") or
     way:has_tag("railway:preserved", "yes") then
    return
  end

  way:set_target_layer("rail")
  way:set_approved()
end


function process_area(area)
  if not area:has_tag("building", "yes") or
     area:has_tag("building", "residential") then
    return
  end

  area:set_target_layer("building")
  area:set_approved()
end
