local sceneTable = {
    {
        GameObject = {
            name = "Terrain",
        },
        MeshRenderer = {
            shaderProgram = "shaders/terrain.glsl",
            mesh = "meshes/terrain.ply",
            texture = "textures/grass.png",
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
            shaderProgram = "shaders/color.glsl",
            mesh = "meshes/cube.ply",
            fields = {
                uColor = { 0.0, 5.0, 100.0 },
            }
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
            shaderProgram = "shaders/color.glsl",
            mesh = "meshes/cube.ply",
            fields = {
                uColor = { 100.0, 0.0, 0.0 },
            }
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
            shaderProgram = "shaders/color.glsl",
            mesh = "meshes/cube.ply",
            fields = {
                uColor = { 0.0, 100.0, 0.0 },
            }
        },
    },
}

local x, z

for x = 0, 20 do
    for z = 0, 10 do
        local mesh, texture

        if (x + z) % 2 == 0 then
            mesh = "meshes/fox.ply"
            texture = "textures/fox_a.png"
        else
            mesh = "meshes/yeen.ply"
            texture = "textures/yeen_a.png"
        end

        table.insert(sceneTable, {
            GameObject = {
                name = "Minion " .. x .. "_" .. z,

                transform = {
                    position = { x * 3.0 - 30.0, 0.0, z * 6.0 - 30.0 },
                },
            },
            MeshRenderer = {
                shaderProgram = "shaders/opaque.glsl",
                mesh = mesh,
                texture = texture,
            },
        })
    end
end

return sceneTable