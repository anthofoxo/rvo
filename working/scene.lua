local sceneTable = {
    {
        GameObject = {
            name = "Terrain",
        },
        MeshRenderer = {
            mesh = "meshes/terrain.ply",
            material = "materials/terrain.lua",
        },
    },
    {
        GameObject = {
            name = "Pine",
        },
        MeshRenderer = {
            mesh = "meshes/pine.ply",
            material = "materials/pine.lua",
        },
    },
    {
        GameObject = {
            name = "Blue Light",
            transform = {
                position = { -8.0, 5.0, -20.0 },
            },
        },
        MeshRenderer = {
            mesh = "meshes/cube.ply",
            material = "materials/light_blue.lua",
        },
    },
    {
        GameObject = {
            name = "Red Light",
            transform = {
                position = { -10.0, 2.0, 10.0 },
            },
        },
        MeshRenderer = {
            mesh = "meshes/cube.ply",
            material = "materials/light_red.lua",
        },
    },
    {
        GameObject = {
            name = "Green Light",
            transform = {
                position = { 30.0, 8.0, 2.0 },
            },
        },
        MeshRenderer = {
            mesh = "meshes/cube.ply",
            material = "materials/light_green.lua",
        },
    },
}

local x, z

for x = 0, 20 do
    for z = 0, 10 do
        local mesh, material

        if (x + z) % 2 == 0 then
            mesh = "meshes/fox.ply"
            material = "materials/fox.lua"
        else
            mesh = "meshes/yeen.ply"
            material = "materials/yeen.lua"
        end

        table.insert(sceneTable, {
            GameObject = {
                name = "Minion " .. x .. "_" .. z,

                transform = {
                    position = { x * 3.0 - 30.0, 0.0, z * 6.0 - 30.0 },
                },
            },
            MeshRenderer = {
                mesh = mesh,
                material = material,
            },
        })
    end
end

return sceneTable