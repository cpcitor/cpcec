// Z80 Disassembler
// Supports main opcodes, CB, DD, ED, FD prefixes

type ReadByte = (addr: number) => number

interface DisassemblyResult {
  instruction: string
  bytes: number[]
  length: number
}

const r8 = ['B', 'C', 'D', 'E', 'H', 'L', '(HL)', 'A']
const r16 = ['BC', 'DE', 'HL', 'SP']
const r16af = ['BC', 'DE', 'HL', 'AF']
const cc = ['NZ', 'Z', 'NC', 'C', 'PO', 'PE', 'P', 'M']
const alu = ['ADD A,', 'ADC A,', 'SUB', 'SBC A,', 'AND', 'XOR', 'OR', 'CP']

function hex8(v: number): string {
  return '$' + v.toString(16).toUpperCase().padStart(2, '0')
}

function hex16(v: number): string {
  return '$' + v.toString(16).toUpperCase().padStart(4, '0')
}

function signedByte(v: number): string {
  const signed = v > 127 ? v - 256 : v
  return signed >= 0 ? `+${signed}` : `${signed}`
}

// CB prefix instructions (bit operations)
function disassembleCB(opcode: number): string {
  const op = (opcode >> 6) & 3
  const bit = (opcode >> 3) & 7
  const reg = opcode & 7

  switch (op) {
    case 0: // Rotates/shifts
      const shifts = ['RLC', 'RRC', 'RL', 'RR', 'SLA', 'SRA', 'SLL', 'SRL']
      return `${shifts[bit]} ${r8[reg]}`
    case 1: return `BIT ${bit},${r8[reg]}`
    case 2: return `RES ${bit},${r8[reg]}`
    case 3: return `SET ${bit},${r8[reg]}`
  }
  return '???'
}

// CB prefix with IX/IY displacement
function disassembleCBIndexed(opcode: number, disp: number, reg: string): string {
  const op = (opcode >> 6) & 3
  const bit = (opcode >> 3) & 7
  const r = opcode & 7
  const indexed = `(${reg}${signedByte(disp)})`

  switch (op) {
    case 0:
      const shifts = ['RLC', 'RRC', 'RL', 'RR', 'SLA', 'SRA', 'SLL', 'SRL']
      if (r === 6) return `${shifts[bit]} ${indexed}`
      return `${shifts[bit]} ${indexed},${r8[r]}`
    case 1: return `BIT ${bit},${indexed}`
    case 2:
      if (r === 6) return `RES ${bit},${indexed}`
      return `RES ${bit},${indexed},${r8[r]}`
    case 3:
      if (r === 6) return `SET ${bit},${indexed}`
      return `SET ${bit},${indexed},${r8[r]}`
  }
  return '???'
}

// ED prefix instructions
function disassembleED(opcode: number, read: ReadByte, addr: number): DisassemblyResult {
  const bytes = [0xED, opcode]

  switch (opcode) {
    case 0x40: return { instruction: 'IN B,(C)', bytes, length: 2 }
    case 0x41: return { instruction: 'OUT (C),B', bytes, length: 2 }
    case 0x42: return { instruction: 'SBC HL,BC', bytes, length: 2 }
    case 0x43: {
      const lo = read((addr + 2) & 0xFFFF)
      const hi = read((addr + 3) & 0xFFFF)
      return { instruction: `LD (${hex16((hi << 8) | lo)}),BC`, bytes: [...bytes, lo, hi], length: 4 }
    }
    case 0x44: return { instruction: 'NEG', bytes, length: 2 }
    case 0x45: return { instruction: 'RETN', bytes, length: 2 }
    case 0x46: return { instruction: 'IM 0', bytes, length: 2 }
    case 0x47: return { instruction: 'LD I,A', bytes, length: 2 }
    case 0x48: return { instruction: 'IN C,(C)', bytes, length: 2 }
    case 0x49: return { instruction: 'OUT (C),C', bytes, length: 2 }
    case 0x4A: return { instruction: 'ADC HL,BC', bytes, length: 2 }
    case 0x4B: {
      const lo = read((addr + 2) & 0xFFFF)
      const hi = read((addr + 3) & 0xFFFF)
      return { instruction: `LD BC,(${hex16((hi << 8) | lo)})`, bytes: [...bytes, lo, hi], length: 4 }
    }
    case 0x4D: return { instruction: 'RETI', bytes, length: 2 }
    case 0x4F: return { instruction: 'LD R,A', bytes, length: 2 }
    case 0x50: return { instruction: 'IN D,(C)', bytes, length: 2 }
    case 0x51: return { instruction: 'OUT (C),D', bytes, length: 2 }
    case 0x52: return { instruction: 'SBC HL,DE', bytes, length: 2 }
    case 0x53: {
      const lo = read((addr + 2) & 0xFFFF)
      const hi = read((addr + 3) & 0xFFFF)
      return { instruction: `LD (${hex16((hi << 8) | lo)}),DE`, bytes: [...bytes, lo, hi], length: 4 }
    }
    case 0x56: return { instruction: 'IM 1', bytes, length: 2 }
    case 0x57: return { instruction: 'LD A,I', bytes, length: 2 }
    case 0x58: return { instruction: 'IN E,(C)', bytes, length: 2 }
    case 0x59: return { instruction: 'OUT (C),E', bytes, length: 2 }
    case 0x5A: return { instruction: 'ADC HL,DE', bytes, length: 2 }
    case 0x5B: {
      const lo = read((addr + 2) & 0xFFFF)
      const hi = read((addr + 3) & 0xFFFF)
      return { instruction: `LD DE,(${hex16((hi << 8) | lo)})`, bytes: [...bytes, lo, hi], length: 4 }
    }
    case 0x5E: return { instruction: 'IM 2', bytes, length: 2 }
    case 0x5F: return { instruction: 'LD A,R', bytes, length: 2 }
    case 0x60: return { instruction: 'IN H,(C)', bytes, length: 2 }
    case 0x61: return { instruction: 'OUT (C),H', bytes, length: 2 }
    case 0x62: return { instruction: 'SBC HL,HL', bytes, length: 2 }
    case 0x63: {
      const lo = read((addr + 2) & 0xFFFF)
      const hi = read((addr + 3) & 0xFFFF)
      return { instruction: `LD (${hex16((hi << 8) | lo)}),HL`, bytes: [...bytes, lo, hi], length: 4 }
    }
    case 0x67: return { instruction: 'RRD', bytes, length: 2 }
    case 0x68: return { instruction: 'IN L,(C)', bytes, length: 2 }
    case 0x69: return { instruction: 'OUT (C),L', bytes, length: 2 }
    case 0x6A: return { instruction: 'ADC HL,HL', bytes, length: 2 }
    case 0x6B: {
      const lo = read((addr + 2) & 0xFFFF)
      const hi = read((addr + 3) & 0xFFFF)
      return { instruction: `LD HL,(${hex16((hi << 8) | lo)})`, bytes: [...bytes, lo, hi], length: 4 }
    }
    case 0x6F: return { instruction: 'RLD', bytes, length: 2 }
    case 0x70: return { instruction: 'IN (C)', bytes, length: 2 }
    case 0x71: return { instruction: 'OUT (C),0', bytes, length: 2 }
    case 0x72: return { instruction: 'SBC HL,SP', bytes, length: 2 }
    case 0x73: {
      const lo = read((addr + 2) & 0xFFFF)
      const hi = read((addr + 3) & 0xFFFF)
      return { instruction: `LD (${hex16((hi << 8) | lo)}),SP`, bytes: [...bytes, lo, hi], length: 4 }
    }
    case 0x78: return { instruction: 'IN A,(C)', bytes, length: 2 }
    case 0x79: return { instruction: 'OUT (C),A', bytes, length: 2 }
    case 0x7A: return { instruction: 'ADC HL,SP', bytes, length: 2 }
    case 0x7B: {
      const lo = read((addr + 2) & 0xFFFF)
      const hi = read((addr + 3) & 0xFFFF)
      return { instruction: `LD SP,(${hex16((hi << 8) | lo)})`, bytes: [...bytes, lo, hi], length: 4 }
    }
    case 0xA0: return { instruction: 'LDI', bytes, length: 2 }
    case 0xA1: return { instruction: 'CPI', bytes, length: 2 }
    case 0xA2: return { instruction: 'INI', bytes, length: 2 }
    case 0xA3: return { instruction: 'OUTI', bytes, length: 2 }
    case 0xA8: return { instruction: 'LDD', bytes, length: 2 }
    case 0xA9: return { instruction: 'CPD', bytes, length: 2 }
    case 0xAA: return { instruction: 'IND', bytes, length: 2 }
    case 0xAB: return { instruction: 'OUTD', bytes, length: 2 }
    case 0xB0: return { instruction: 'LDIR', bytes, length: 2 }
    case 0xB1: return { instruction: 'CPIR', bytes, length: 2 }
    case 0xB2: return { instruction: 'INIR', bytes, length: 2 }
    case 0xB3: return { instruction: 'OTIR', bytes, length: 2 }
    case 0xB8: return { instruction: 'LDDR', bytes, length: 2 }
    case 0xB9: return { instruction: 'CPDR', bytes, length: 2 }
    case 0xBA: return { instruction: 'INDR', bytes, length: 2 }
    case 0xBB: return { instruction: 'OTDR', bytes, length: 2 }
    default:
      return { instruction: `DB $ED,${hex8(opcode)}`, bytes, length: 2 }
  }
}

// DD/FD prefix (IX/IY instructions)
function disassembleIndexed(prefix: number, read: ReadByte, addr: number): DisassemblyResult {
  const reg = prefix === 0xDD ? 'IX' : 'IY'
  const regH = prefix === 0xDD ? 'IXH' : 'IYH'
  const regL = prefix === 0xDD ? 'IXL' : 'IYL'
  const opcode = read((addr + 1) & 0xFFFF)
  const bytes = [prefix, opcode]

  // Handle DD CB / FD CB prefix
  if (opcode === 0xCB) {
    const disp = read((addr + 2) & 0xFFFF)
    const cb = read((addr + 3) & 0xFFFF)
    return {
      instruction: disassembleCBIndexed(cb, disp, reg),
      bytes: [prefix, 0xCB, disp, cb],
      length: 4
    }
  }

  // Get displacement for indexed operations
  const getDisp = () => {
    const d = read((addr + 2) & 0xFFFF)
    bytes.push(d)
    return d
  }

  switch (opcode) {
    case 0x09: return { instruction: `ADD ${reg},BC`, bytes, length: 2 }
    case 0x19: return { instruction: `ADD ${reg},DE`, bytes, length: 2 }
    case 0x21: {
      const lo = read((addr + 2) & 0xFFFF)
      const hi = read((addr + 3) & 0xFFFF)
      return { instruction: `LD ${reg},${hex16((hi << 8) | lo)}`, bytes: [...bytes, lo, hi], length: 4 }
    }
    case 0x22: {
      const lo = read((addr + 2) & 0xFFFF)
      const hi = read((addr + 3) & 0xFFFF)
      return { instruction: `LD (${hex16((hi << 8) | lo)}),${reg}`, bytes: [...bytes, lo, hi], length: 4 }
    }
    case 0x23: return { instruction: `INC ${reg}`, bytes, length: 2 }
    case 0x24: return { instruction: `INC ${regH}`, bytes, length: 2 }
    case 0x25: return { instruction: `DEC ${regH}`, bytes, length: 2 }
    case 0x26: {
      const n = read((addr + 2) & 0xFFFF)
      return { instruction: `LD ${regH},${hex8(n)}`, bytes: [...bytes, n], length: 3 }
    }
    case 0x29: return { instruction: `ADD ${reg},${reg}`, bytes, length: 2 }
    case 0x2A: {
      const lo = read((addr + 2) & 0xFFFF)
      const hi = read((addr + 3) & 0xFFFF)
      return { instruction: `LD ${reg},(${hex16((hi << 8) | lo)})`, bytes: [...bytes, lo, hi], length: 4 }
    }
    case 0x2B: return { instruction: `DEC ${reg}`, bytes, length: 2 }
    case 0x2C: return { instruction: `INC ${regL}`, bytes, length: 2 }
    case 0x2D: return { instruction: `DEC ${regL}`, bytes, length: 2 }
    case 0x2E: {
      const n = read((addr + 2) & 0xFFFF)
      return { instruction: `LD ${regL},${hex8(n)}`, bytes: [...bytes, n], length: 3 }
    }
    case 0x34: {
      const d = getDisp()
      return { instruction: `INC (${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0x35: {
      const d = getDisp()
      return { instruction: `DEC (${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0x36: {
      const d = read((addr + 2) & 0xFFFF)
      const n = read((addr + 3) & 0xFFFF)
      return { instruction: `LD (${reg}${signedByte(d)}),${hex8(n)}`, bytes: [...bytes, d, n], length: 4 }
    }
    case 0x39: return { instruction: `ADD ${reg},SP`, bytes, length: 2 }
    case 0x44: return { instruction: `LD B,${regH}`, bytes, length: 2 }
    case 0x45: return { instruction: `LD B,${regL}`, bytes, length: 2 }
    case 0x46: {
      const d = getDisp()
      return { instruction: `LD B,(${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0x4C: return { instruction: `LD C,${regH}`, bytes, length: 2 }
    case 0x4D: return { instruction: `LD C,${regL}`, bytes, length: 2 }
    case 0x4E: {
      const d = getDisp()
      return { instruction: `LD C,(${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0x54: return { instruction: `LD D,${regH}`, bytes, length: 2 }
    case 0x55: return { instruction: `LD D,${regL}`, bytes, length: 2 }
    case 0x56: {
      const d = getDisp()
      return { instruction: `LD D,(${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0x5C: return { instruction: `LD E,${regH}`, bytes, length: 2 }
    case 0x5D: return { instruction: `LD E,${regL}`, bytes, length: 2 }
    case 0x5E: {
      const d = getDisp()
      return { instruction: `LD E,(${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0x60: return { instruction: `LD ${regH},B`, bytes, length: 2 }
    case 0x61: return { instruction: `LD ${regH},C`, bytes, length: 2 }
    case 0x62: return { instruction: `LD ${regH},D`, bytes, length: 2 }
    case 0x63: return { instruction: `LD ${regH},E`, bytes, length: 2 }
    case 0x64: return { instruction: `LD ${regH},${regH}`, bytes, length: 2 }
    case 0x65: return { instruction: `LD ${regH},${regL}`, bytes, length: 2 }
    case 0x66: {
      const d = getDisp()
      return { instruction: `LD H,(${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0x67: return { instruction: `LD ${regH},A`, bytes, length: 2 }
    case 0x68: return { instruction: `LD ${regL},B`, bytes, length: 2 }
    case 0x69: return { instruction: `LD ${regL},C`, bytes, length: 2 }
    case 0x6A: return { instruction: `LD ${regL},D`, bytes, length: 2 }
    case 0x6B: return { instruction: `LD ${regL},E`, bytes, length: 2 }
    case 0x6C: return { instruction: `LD ${regL},${regH}`, bytes, length: 2 }
    case 0x6D: return { instruction: `LD ${regL},${regL}`, bytes, length: 2 }
    case 0x6E: {
      const d = getDisp()
      return { instruction: `LD L,(${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0x6F: return { instruction: `LD ${regL},A`, bytes, length: 2 }
    case 0x70: {
      const d = getDisp()
      return { instruction: `LD (${reg}${signedByte(d)}),B`, bytes, length: 3 }
    }
    case 0x71: {
      const d = getDisp()
      return { instruction: `LD (${reg}${signedByte(d)}),C`, bytes, length: 3 }
    }
    case 0x72: {
      const d = getDisp()
      return { instruction: `LD (${reg}${signedByte(d)}),D`, bytes, length: 3 }
    }
    case 0x73: {
      const d = getDisp()
      return { instruction: `LD (${reg}${signedByte(d)}),E`, bytes, length: 3 }
    }
    case 0x74: {
      const d = getDisp()
      return { instruction: `LD (${reg}${signedByte(d)}),H`, bytes, length: 3 }
    }
    case 0x75: {
      const d = getDisp()
      return { instruction: `LD (${reg}${signedByte(d)}),L`, bytes, length: 3 }
    }
    case 0x77: {
      const d = getDisp()
      return { instruction: `LD (${reg}${signedByte(d)}),A`, bytes, length: 3 }
    }
    case 0x7C: return { instruction: `LD A,${regH}`, bytes, length: 2 }
    case 0x7D: return { instruction: `LD A,${regL}`, bytes, length: 2 }
    case 0x7E: {
      const d = getDisp()
      return { instruction: `LD A,(${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0x84: return { instruction: `ADD A,${regH}`, bytes, length: 2 }
    case 0x85: return { instruction: `ADD A,${regL}`, bytes, length: 2 }
    case 0x86: {
      const d = getDisp()
      return { instruction: `ADD A,(${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0x8C: return { instruction: `ADC A,${regH}`, bytes, length: 2 }
    case 0x8D: return { instruction: `ADC A,${regL}`, bytes, length: 2 }
    case 0x8E: {
      const d = getDisp()
      return { instruction: `ADC A,(${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0x94: return { instruction: `SUB ${regH}`, bytes, length: 2 }
    case 0x95: return { instruction: `SUB ${regL}`, bytes, length: 2 }
    case 0x96: {
      const d = getDisp()
      return { instruction: `SUB (${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0x9C: return { instruction: `SBC A,${regH}`, bytes, length: 2 }
    case 0x9D: return { instruction: `SBC A,${regL}`, bytes, length: 2 }
    case 0x9E: {
      const d = getDisp()
      return { instruction: `SBC A,(${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0xA4: return { instruction: `AND ${regH}`, bytes, length: 2 }
    case 0xA5: return { instruction: `AND ${regL}`, bytes, length: 2 }
    case 0xA6: {
      const d = getDisp()
      return { instruction: `AND (${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0xAC: return { instruction: `XOR ${regH}`, bytes, length: 2 }
    case 0xAD: return { instruction: `XOR ${regL}`, bytes, length: 2 }
    case 0xAE: {
      const d = getDisp()
      return { instruction: `XOR (${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0xB4: return { instruction: `OR ${regH}`, bytes, length: 2 }
    case 0xB5: return { instruction: `OR ${regL}`, bytes, length: 2 }
    case 0xB6: {
      const d = getDisp()
      return { instruction: `OR (${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0xBC: return { instruction: `CP ${regH}`, bytes, length: 2 }
    case 0xBD: return { instruction: `CP ${regL}`, bytes, length: 2 }
    case 0xBE: {
      const d = getDisp()
      return { instruction: `CP (${reg}${signedByte(d)})`, bytes, length: 3 }
    }
    case 0xE1: return { instruction: `POP ${reg}`, bytes, length: 2 }
    case 0xE3: return { instruction: `EX (SP),${reg}`, bytes, length: 2 }
    case 0xE5: return { instruction: `PUSH ${reg}`, bytes, length: 2 }
    case 0xE9: return { instruction: `JP (${reg})`, bytes, length: 2 }
    case 0xF9: return { instruction: `LD SP,${reg}`, bytes, length: 2 }
    default:
      return { instruction: `DB ${hex8(prefix)},${hex8(opcode)}`, bytes, length: 2 }
  }
}

// Main instruction decoder
export function disassembleInstruction(read: ReadByte, addr: number): DisassemblyResult {
  const opcode = read(addr)
  const bytes = [opcode]

  // Prefixes
  if (opcode === 0xCB) {
    const cb = read((addr + 1) & 0xFFFF)
    return { instruction: disassembleCB(cb), bytes: [0xCB, cb], length: 2 }
  }

  if (opcode === 0xDD || opcode === 0xFD) {
    return disassembleIndexed(opcode, read, addr)
  }

  if (opcode === 0xED) {
    return disassembleED(read((addr + 1) & 0xFFFF), read, addr)
  }

  // Decode main opcode
  const x = (opcode >> 6) & 3
  const y = (opcode >> 3) & 7
  const z = opcode & 7
  const p = (y >> 1) & 3
  const q = y & 1

  if (x === 0) {
    switch (z) {
      case 0:
        switch (y) {
          case 0: return { instruction: 'NOP', bytes, length: 1 }
          case 1: return { instruction: "EX AF,AF'", bytes, length: 1 }
          case 2: {
            const d = read((addr + 1) & 0xFFFF)
            const target = (addr + 2 + (d > 127 ? d - 256 : d)) & 0xFFFF
            return { instruction: `DJNZ ${hex16(target)}`, bytes: [...bytes, d], length: 2 }
          }
          case 3: {
            const d = read((addr + 1) & 0xFFFF)
            const target = (addr + 2 + (d > 127 ? d - 256 : d)) & 0xFFFF
            return { instruction: `JR ${hex16(target)}`, bytes: [...bytes, d], length: 2 }
          }
          default: {
            const d = read((addr + 1) & 0xFFFF)
            const target = (addr + 2 + (d > 127 ? d - 256 : d)) & 0xFFFF
            return { instruction: `JR ${cc[y - 4]},${hex16(target)}`, bytes: [...bytes, d], length: 2 }
          }
        }
      case 1:
        if (q === 0) {
          const lo = read((addr + 1) & 0xFFFF)
          const hi = read((addr + 2) & 0xFFFF)
          return { instruction: `LD ${r16[p]},${hex16((hi << 8) | lo)}`, bytes: [...bytes, lo, hi], length: 3 }
        } else {
          return { instruction: `ADD HL,${r16[p]}`, bytes, length: 1 }
        }
      case 2:
        if (q === 0) {
          switch (p) {
            case 0: return { instruction: 'LD (BC),A', bytes, length: 1 }
            case 1: return { instruction: 'LD (DE),A', bytes, length: 1 }
            case 2: {
              const lo = read((addr + 1) & 0xFFFF)
              const hi = read((addr + 2) & 0xFFFF)
              return { instruction: `LD (${hex16((hi << 8) | lo)}),HL`, bytes: [...bytes, lo, hi], length: 3 }
            }
            case 3: {
              const lo = read((addr + 1) & 0xFFFF)
              const hi = read((addr + 2) & 0xFFFF)
              return { instruction: `LD (${hex16((hi << 8) | lo)}),A`, bytes: [...bytes, lo, hi], length: 3 }
            }
          }
        } else {
          switch (p) {
            case 0: return { instruction: 'LD A,(BC)', bytes, length: 1 }
            case 1: return { instruction: 'LD A,(DE)', bytes, length: 1 }
            case 2: {
              const lo = read((addr + 1) & 0xFFFF)
              const hi = read((addr + 2) & 0xFFFF)
              return { instruction: `LD HL,(${hex16((hi << 8) | lo)})`, bytes: [...bytes, lo, hi], length: 3 }
            }
            case 3: {
              const lo = read((addr + 1) & 0xFFFF)
              const hi = read((addr + 2) & 0xFFFF)
              return { instruction: `LD A,(${hex16((hi << 8) | lo)})`, bytes: [...bytes, lo, hi], length: 3 }
            }
          }
        }
        break
      case 3:
        return { instruction: q === 0 ? `INC ${r16[p]}` : `DEC ${r16[p]}`, bytes, length: 1 }
      case 4:
        return { instruction: `INC ${r8[y]}`, bytes, length: 1 }
      case 5:
        return { instruction: `DEC ${r8[y]}`, bytes, length: 1 }
      case 6: {
        const n = read((addr + 1) & 0xFFFF)
        return { instruction: `LD ${r8[y]},${hex8(n)}`, bytes: [...bytes, n], length: 2 }
      }
      case 7: {
        const ops = ['RLCA', 'RRCA', 'RLA', 'RRA', 'DAA', 'CPL', 'SCF', 'CCF']
        return { instruction: ops[y], bytes, length: 1 }
      }
    }
  }

  if (x === 1) {
    if (y === 6 && z === 6) {
      return { instruction: 'HALT', bytes, length: 1 }
    }
    return { instruction: `LD ${r8[y]},${r8[z]}`, bytes, length: 1 }
  }

  if (x === 2) {
    return { instruction: `${alu[y]} ${r8[z]}`, bytes, length: 1 }
  }

  // x === 3
  switch (z) {
    case 0:
      return { instruction: `RET ${cc[y]}`, bytes, length: 1 }
    case 1:
      if (q === 0) {
        return { instruction: `POP ${r16af[p]}`, bytes, length: 1 }
      } else {
        switch (p) {
          case 0: return { instruction: 'RET', bytes, length: 1 }
          case 1: return { instruction: 'EXX', bytes, length: 1 }
          case 2: return { instruction: 'JP (HL)', bytes, length: 1 }
          case 3: return { instruction: 'LD SP,HL', bytes, length: 1 }
        }
      }
      break
    case 2: {
      const lo = read((addr + 1) & 0xFFFF)
      const hi = read((addr + 2) & 0xFFFF)
      return { instruction: `JP ${cc[y]},${hex16((hi << 8) | lo)}`, bytes: [...bytes, lo, hi], length: 3 }
    }
    case 3:
      switch (y) {
        case 0: {
          const lo = read((addr + 1) & 0xFFFF)
          const hi = read((addr + 2) & 0xFFFF)
          return { instruction: `JP ${hex16((hi << 8) | lo)}`, bytes: [...bytes, lo, hi], length: 3 }
        }
        case 2: {
          const n = read((addr + 1) & 0xFFFF)
          return { instruction: `OUT (${hex8(n)}),A`, bytes: [...bytes, n], length: 2 }
        }
        case 3: {
          const n = read((addr + 1) & 0xFFFF)
          return { instruction: `IN A,(${hex8(n)})`, bytes: [...bytes, n], length: 2 }
        }
        case 4: return { instruction: 'EX (SP),HL', bytes, length: 1 }
        case 5: return { instruction: 'EX DE,HL', bytes, length: 1 }
        case 6: return { instruction: 'DI', bytes, length: 1 }
        case 7: return { instruction: 'EI', bytes, length: 1 }
      }
      break
    case 4: {
      const lo = read((addr + 1) & 0xFFFF)
      const hi = read((addr + 2) & 0xFFFF)
      return { instruction: `CALL ${cc[y]},${hex16((hi << 8) | lo)}`, bytes: [...bytes, lo, hi], length: 3 }
    }
    case 5:
      if (q === 0) {
        return { instruction: `PUSH ${r16af[p]}`, bytes, length: 1 }
      } else if (p === 0) {
        const lo = read((addr + 1) & 0xFFFF)
        const hi = read((addr + 2) & 0xFFFF)
        return { instruction: `CALL ${hex16((hi << 8) | lo)}`, bytes: [...bytes, lo, hi], length: 3 }
      }
      break
    case 6: {
      const n = read((addr + 1) & 0xFFFF)
      return { instruction: `${alu[y]} ${hex8(n)}`, bytes: [...bytes, n], length: 2 }
    }
    case 7:
      return { instruction: `RST ${hex8(y * 8)}`, bytes, length: 1 }
  }

  return { instruction: `DB ${hex8(opcode)}`, bytes, length: 1 }
}

// Disassemble multiple instructions
export function disassemble(read: ReadByte, startAddr: number, count: number): DisassemblyResult[] {
  const results: DisassemblyResult[] = []
  let addr = startAddr

  for (let i = 0; i < count; i++) {
    const result = disassembleInstruction(read, addr)
    results.push({ ...result, address: addr } as DisassemblyResult & { address: number })
    addr = (addr + result.length) & 0xFFFF
  }

  return results
}
