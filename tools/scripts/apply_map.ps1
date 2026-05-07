\ = Get-Content map_array_generated.txt -Raw
\ = Get-Content map.cpp -Raw

\ = \ -replace 'static std::vector<MapObject> g_MapObjects;', \
\ = \ -replace 'resolvedTex = obj.TextureId;
    }

    switch \(obj.KindId\)', 'resolvedTex = obj.TextureId;
    }

    Direct3D_SetDepthEnable(true);

    switch (obj.KindId)'

[System.IO.File]::WriteAllText("map.cpp", \, [System.Text.Encoding]::UTF8)
