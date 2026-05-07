\ = Get-Content map_array_generated.txt -Raw
\ = Get-Content map.cpp -Raw
\ = \ -replace 'static std::vector<MapObject> g_MapObjects;', \
[System.IO.File]::WriteAllText("map.cpp", \, [System.Text.Encoding]::UTF8)
