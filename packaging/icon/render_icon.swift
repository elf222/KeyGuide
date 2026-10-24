// Renders an SVG to square PNGs: swift render_icon.swift <icon.svg> <output dir> <size>...
// Uses AppKit's SVG support (macOS 14+), so no third-party tools are needed.
import AppKit

let arguments = CommandLine.arguments
guard arguments.count >= 4, let image = NSImage(contentsOf: URL(fileURLWithPath: arguments[1]))
else {
    FileHandle.standardError.write("usage: render_icon.swift <icon.svg> <output dir> <size>...\n".data(using: .utf8)!)
    exit(1)
}
let output = URL(fileURLWithPath: arguments[2], isDirectory: true)
for size in arguments[3...].compactMap({ Int($0) }) {
    guard let bitmap = NSBitmapImageRep(
        bitmapDataPlanes: nil, pixelsWide: size, pixelsHigh: size, bitsPerSample: 8,
        samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB,
        bytesPerRow: 0, bitsPerPixel: 0)
    else {
        exit(1)
    }
    bitmap.size = NSSize(width: size, height: size)
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
    NSGraphicsContext.current?.imageInterpolation = .high
    image.draw(in: NSRect(x: 0, y: 0, width: size, height: size))
    NSGraphicsContext.restoreGraphicsState()
    let file = output.appendingPathComponent("\(size).png")
    try bitmap.representation(using: .png, properties: [:])!.write(to: file)
}
