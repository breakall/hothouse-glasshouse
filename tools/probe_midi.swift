import Foundation
import CoreMIDI

// Query Glasshouse's identity without changing its sound or stored settings.
func name(_ object: MIDIObjectRef) -> String {
    var value: Unmanaged<CFString>?
    guard MIDIObjectGetStringProperty(object, kMIDIPropertyDisplayName, &value) == noErr,
          let string = value else { return "unknown" }
    return string.takeRetainedValue() as String
}

var client = MIDIClientRef()
var input = MIDIPortRef()
var output = MIDIPortRef()
let lock = NSLock()
var verified = false
guard MIDIClientCreate("Glasshouse Probe" as CFString, nil, nil, &client) == noErr else {
    fatalError("Cannot create MIDI client")
}
defer { MIDIClientDispose(client) }
guard MIDIInputPortCreateWithBlock(client, "Identity reply" as CFString, &input, { list, _ in
    var pointer = UnsafeMutableRawPointer(mutating: list)
        .advanced(by: MemoryLayout<MIDIPacketList>.offset(of: \.packet)!)
        .assumingMemoryBound(to: MIDIPacket.self)
    for _ in 0..<list.pointee.numPackets {
        let packet = pointer.pointee
        let count = Int(packet.length)
        let bytes = withUnsafeBytes(of: packet.data) { Array($0.prefix(count)) }
        if bytes.count >= 3 {
            for i in 0...(bytes.count - 3) {
                if bytes[i] == 0xb0 && bytes[i + 1] == 118 && bytes[i + 2] == 2 {
                    lock.lock(); verified = true; lock.unlock()
                }
            }
        }
        pointer = MIDIPacketNext(pointer)
    }
}) == noErr else { fatalError("Cannot open MIDI input") }
guard MIDIOutputPortCreate(client, "Identity query" as CFString, &output) == noErr else {
    fatalError("Cannot open MIDI output")
}

var sources = 0
for i in 0..<MIDIGetNumberOfSources() {
    let endpoint = MIDIGetSource(i)
    let label = name(endpoint)
    if label.localizedCaseInsensitiveContains("Daisy") || label.localizedCaseInsensitiveContains("Glasshouse") {
        print("MIDI source: \(label)")
        MIDIPortConnectSource(input, endpoint, nil)
        sources += 1
    }
}
var destinations: [MIDIEndpointRef] = []
for i in 0..<MIDIGetNumberOfDestinations() {
    let endpoint = MIDIGetDestination(i)
    let label = name(endpoint)
    if label.localizedCaseInsensitiveContains("Daisy") || label.localizedCaseInsensitiveContains("Glasshouse") {
        print("MIDI destination: \(label)")
        destinations.append(endpoint)
    }
}
if sources == 0 || destinations.isEmpty {
    fputs("No Daisy/Glasshouse MIDI endpoints found\n", stderr)
    exit(2)
}

let allocation = UnsafeMutableRawPointer.allocate(byteCount: 1024, alignment: 16)
defer { allocation.deallocate() }
let list = allocation.bindMemory(to: MIDIPacketList.self, capacity: 1)
list.initialize(to: MIDIPacketList())
let packet = MIDIPacketListInit(list)
let message: [UInt8] = [0xb0, 118, 127]
message.withUnsafeBufferPointer { bytes in
    _ = MIDIPacketListAdd(list, 1024, packet, 0, bytes.count, bytes.baseAddress!)
}
for _ in 0..<5 {
    for endpoint in destinations { MIDISend(output, endpoint, list) }
    RunLoop.current.run(until: Date().addingTimeInterval(0.4))
    lock.lock(); let success = verified; lock.unlock()
    if success { print("VERIFIED: Glasshouse firmware identity 2, USB MIDI round trip successful"); exit(0) }
}
fputs("MIDI endpoints exist but no Glasshouse identity response was received\n", stderr)
exit(1)
