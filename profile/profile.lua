

function process_node(node)

  if node:has_tag("place", "city") then
    print(node:get_id())

    node:set_target_layer("cities")
    node:set_approved()
  end

end
