$output = "c:\Users\a3877\OneDrive\桌面\就職活動\simulator\resource\model\sphere\sphere.obj"
$stacks = 16
$slices = 32
$radius = 1.0
$PI = [math]::PI

$obj = "o Sphere`n"

for ($i = 0; $i -le $stacks; $i++) {
    $v = $i / $stacks
    $phi = $v * $PI
    for ($j = 0; $j -le $slices; $j++) {
        $u = $j / $slices
        $theta = $u * 2.0 * $PI
        
        $x = [math]::Cos($theta) * [math]::Sin($phi)
        $y = [math]::Cos($phi)
        $z = [math]::Sin($theta) * [math]::Sin($phi)
        
        $px = $x * $radius
        $py = $y * $radius
        $pz = $z * $radius
        
        $obj += "v $px $py $pz`n"
        $obj += "vn $x $y $z`n"
        $vt_y = 1.0 - $v
        $obj += "vt $u $vt_y`n"
    }
}

for ($i = 0; $i -lt $stacks; $i++) {
    for ($j = 0; $j -lt $slices; $j++) {
        $p1 = $i * ($slices + 1) + $j + 1
        $p2 = $p1 + 1
        $p3 = $p1 + ($slices + 1)
        $p4 = $p3 + 1
        $obj += "f $p1/$p1/$p1 $p2/$p2/$p2 $p4/$p4/$p4 $p3/$p3/$p3`n"
    }
}

[System.IO.File]::WriteAllText($output, $obj)
