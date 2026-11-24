import Foundation
import CoreMIDI

// Temporary diagnostic firmware only. Reads live controls; optionally sets Attack.
// Usage: inspect_controls [seconds] [attack 0..127]
let arguments = Array(CommandLine.arguments.dropFirst())
let duration = arguments.first.flatMap(Double.init) ?? 10
let attack: UInt8? = arguments.count > 1 ? UInt8(arguments[1]) : nil
guard duration > 0 && duration <= 300 && arguments.count <= 2,
      arguments.count < 2 || (attack != nil && attack! < 128) else {
    fputs("Usage: inspect_controls [seconds 1..300] [attack 0..127]\n", stderr); exit(2)
}
func name(_ object: MIDIObjectRef) -> String {
    var value: Unmanaged<CFString>?
    guard MIDIObjectGetStringProperty(object, kMIDIPropertyDisplayName, &value) == noErr,
          let text = value else { return "unknown" }
    return text.takeRetainedValue() as String
}
func matches(_ object: MIDIObjectRef) -> Bool {
    let label = name(object)
    return label.localizedCaseInsensitiveContains("Daisy") || label.localizedCaseInsensitiveContains("Glasshouse")
}
var client = MIDIClientRef(), input = MIDIPortRef(), output = MIDIPortRef()
let lock = NSLock()
var values: [UInt8: UInt8] = [:]
var identity: UInt8?
var completed = false
guard MIDIClientCreate("Glasshouse Diagnostics" as CFString, nil, nil, &client) == noErr else { exit(2) }
defer { MIDIClientDispose(client) }
guard MIDIInputPortCreateWithBlock(client, "Snapshot" as CFString, &input, { list, _ in
    var pointer = UnsafeMutableRawPointer(mutating: list)
        .advanced(by: MemoryLayout<MIDIPacketList>.offset(of: \.packet)!)
        .assumingMemoryBound(to: MIDIPacket.self)
    lock.lock(); defer { lock.unlock() }
    for _ in 0..<list.pointee.numPackets {
        let packet = pointer.pointee
        let bytes = withUnsafeBytes(of: packet.data) { Array($0.prefix(Int(packet.length))) }
        if bytes.count >= 3 {
            for i in 0...(bytes.count - 3) where bytes[i] == 0xb0 && bytes[i + 1] < 128 && bytes[i + 2] < 128 {
                let cc = bytes[i + 1], value = bytes[i + 2]
                if cc == 118 { identity = value }
                else if cc == 117 && value == 3 { completed = true }
                else if cc >= 60 && cc <= 80 { values[cc] = value }
            }
        }
        pointer = MIDIPacketNext(pointer)
    }
}) == noErr,
MIDIOutputPortCreate(client, "Query" as CFString, &output) == noErr else { exit(2) }
let sources = (0..<MIDIGetNumberOfSources()).map(MIDIGetSource).filter(matches)
let destinations = (0..<MIDIGetNumberOfDestinations()).map(MIDIGetDestination).filter(matches)
guard sources.count == 1 && destinations.count == 1 else {
    fputs("Expected exactly one Daisy/Glasshouse source and destination\n", stderr); exit(2)
}
MIDIPortConnectSource(input, sources[0], nil)
func send(_ cc: UInt8, _ value: UInt8) {
    let allocation = UnsafeMutableRawPointer.allocate(byteCount: 1024, alignment: 16)
    defer { allocation.deallocate() }
    let list = allocation.bindMemory(to: MIDIPacketList.self, capacity: 1)
    list.initialize(to: MIDIPacketList())
    let packet = MIDIPacketListInit(list)
    let bytes: [UInt8] = [0xb0, cc, value]
    bytes.withUnsafeBufferPointer { buffer in
        _ = MIDIPacketListAdd(list, 1024, packet, 0, buffer.count, buffer.baseAddress!)
    }
    guard MIDISend(output, destinations[0], list) == noErr else { exit(3) }
}
send(118, 127)
RunLoop.current.run(until: Date().addingTimeInterval(1))
lock.lock(); let firmware = identity; lock.unlock()
guard firmware == 3 else {
    fputs("Diagnostic firmware identity 3 required; no settings changed\n", stderr); exit(3)
}
print("Connected: \(name(destinations[0])) — diagnostic firmware 3")
if let value = attack { send(17, value); print("Set Attack CC 17 = \(value)") }
let start = Date()
var received = 0
while Date().timeIntervalSince(start) < duration {
    lock.lock(); values.removeAll(); completed = false; lock.unlock()
    send(117, 127)
    RunLoop.current.run(until: Date().addingTimeInterval(0.1))
    lock.lock(); let snapshot = values; let ready = completed; lock.unlock()
    guard ready && snapshot.count == 21 else { continue }
    received += 1
    let physical = (60...65).map { String(snapshot[UInt8($0)]!) }.joined(separator: ",")
    let active = (66...71).map { String(snapshot[UInt8($0)]!) }.joined(separator: ",")
    let attackMs = Double(Int(snapshot[76]!) * 128 + Int(snapshot[77]!)) / 10
    let releaseMs = Double(Int(snapshot[79]!) * 128 + Int(snapshot[80]!)) / 10
    let peakDb = Double(snapshot[78]!) * 96 / 127 - 96
    let reductionDb = Double(snapshot[75]!) * 72 / 127
    print(String(format: "t=%.1f physical=[%@] active=[%@] page=%d bypass=%d env=%d attack=%.1fms release=%.1fms peak=%.1fdBFS GR=%.1fdB",
                 Date().timeIntervalSince(start), physical, active, Int(snapshot[72]!), Int(snapshot[73]!), Int(snapshot[74]!), attackMs, releaseMs, peakDb, reductionDb))
    fflush(stdout)
}
guard received > 0 else { fputs("No complete diagnostic snapshots received\n", stderr); exit(4) }
