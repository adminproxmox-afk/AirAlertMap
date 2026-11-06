param([string]$Destination = (Join-Path $PSScriptRoot '..\resources\AirAlertMap.ico'))
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
# Original geometric mark, matching resources/AirAlertMap.svg. Write classic
# 32-bit DIB frames so both brcc32 and Windows Explorer can read every size.
$frames = @()
foreach ($size in @(16,24,32,48,64,128,256)) {
    $bitmap = [Drawing.Bitmap]::new($size,$size,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.ScaleTransform($size / 256.0, $size / 256.0)
    $navy = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#102a43'))
    $blue = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#197da3'))
    $gold = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#f4c65a'))
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    $path.AddArc(8,8,104,104,180,90); $path.AddArc(144,8,104,104,270,90)
    $path.AddArc(144,144,104,104,0,90); $path.AddArc(8,144,104,104,90,90); $path.CloseFigure()
    $graphics.FillPath($navy,$path)
    $shield = [Drawing.Drawing2D.GraphicsPath]::new()
    $shield.AddLines([Drawing.PointF[]]@([Drawing.PointF]::new(128,40),[Drawing.PointF]::new(206,70),[Drawing.PointF]::new(206,125)))
    $shield.AddBezier(206,125,206,168,176,199,128,224)
    $shield.AddBezier(128,224,80,199,50,168,50,125)
    $shield.AddLine(50,125,50,70); $shield.CloseFigure()
    $graphics.FillPath($blue,$shield)
    $graphics.FillPolygon($gold,[Drawing.PointF[]]@([Drawing.PointF]::new(128,70),[Drawing.PointF]::new(178,174),[Drawing.PointF]::new(146,174),[Drawing.PointF]::new(128,135),[Drawing.PointF]::new(110,174),[Drawing.PointF]::new(78,174)))
    $graphics.FillEllipse([Drawing.Brushes]::White,118,180,20,20)
    $stream = [IO.MemoryStream]::new(); $writer = [IO.BinaryWriter]::new($stream)
    $maskStride = [int]([Math]::Ceiling($size / 32.0) * 4)
    $writer.Write([int]40); $writer.Write([int]$size); $writer.Write([int]($size*2))
    $writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([int]0)
    $writer.Write([int]($size*$size*4)); 1..4 | ForEach-Object { $writer.Write([int]0) }
    for ($y=$size-1; $y -ge 0; $y--) {
        for ($x=0; $x -lt $size; $x++) {
            $pixel=$bitmap.GetPixel($x,$y)
            $writer.Write([byte]$pixel.B); $writer.Write([byte]$pixel.G); $writer.Write([byte]$pixel.R); $writer.Write([byte]$pixel.A)
        }
    }
    $writer.Write([byte[]]::new($maskStride*$size))
    $frames += ,@{ Size=$size; Data=$stream.ToArray() }
    $writer.Dispose(); $stream.Dispose(); $shield.Dispose(); $path.Dispose()
    $navy.Dispose(); $blue.Dispose(); $gold.Dispose(); $graphics.Dispose(); $bitmap.Dispose()
}
$destinationPath = [IO.Path]::GetFullPath($Destination)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destinationPath)) | Out-Null
$output = [IO.File]::Create($destinationPath); $writer=[IO.BinaryWriter]::new($output)
try {
    $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$frames.Count)
    $offset=6+16*$frames.Count
    foreach ($frame in $frames) {
        $dimension=if ($frame.Size -eq 256) { 0 } else { $frame.Size }
        $writer.Write([byte]$dimension); $writer.Write([byte]$dimension); $writer.Write([byte]0); $writer.Write([byte]0)
        $writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([int]$frame.Data.Length); $writer.Write([int]$offset)
        $offset += $frame.Data.Length
    }
    foreach ($frame in $frames) { $writer.Write([byte[]]$frame.Data) }
} finally { $writer.Dispose(); $output.Dispose() }
Write-Host "Icon: $destinationPath (7 sizes, 16-256 px)"
