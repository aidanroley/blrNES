#include "cpu.h"
#include "bus.h"
#include <vector>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <functional>
#include <unordered_map>

int main() {
    cpu myCpu;
    myCpu.run(); // Start the CPU emulation
    return 0; // Return success
}

void cpu::run() {
    loadIntelHexFile("C:/Users/x/Downloads/functional.hex");
    while (true) {
        byte opcode = fetch();
        printCount++;
        //if (printCount < 2000) {
        std::cout << "Opcode: 0x" << std::hex << static_cast<int>(opcode) << " " << std::dec << printCount << std::endl;
        //}
        // Converts hexadecimal to string for readability
        std::stringstream ss;
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(opcode);
        std::string opcodeStr = ss.str();
        for (const auto& info : opcodeTable) {
            if (info.origOpcode == opcodeStr) {
                cycles = info.cycles;
                addressingMode = info.addressingMode;
                instruction = info.instruction;
                break;
            }
        }


        decodeAndExecute(instruction, cycles, addressingMode);
    }
}

// Fetches instruction from an address, returns the opcode, increments program counter
byte cpu::fetch() {
    byte opcode = bus.readBus(pc);
    pc++;
    return opcode;
}

// Addressing mode methods, keep in mind IMP doesn't need one 

// Increments pc after current value is taken
byte cpu::IMMe() {
    byte operand = bus.readBus(pc);
    pc++;
    return operand;
}

byte cpu::ZEROe() {
    zeroPageAddr = bus.readBus(pc++);
    return zeroPageAddr;

}

byte cpu::ZEROXe() {
    zeroPageAddr = bus.readBus(pc);
    zeroPageAddr = (zeroPageAddr + x) & 0xFF;
    pc++;
    return zeroPageAddr;
}

byte cpu::ZEROYe() {
    zeroPageAddr = bus.readBus(pc);
    zeroPageAddr = (zeroPageAddr + y) & 0xFF;
    pc++;
    return zeroPageAddr;

}

// Not sure if I will need the bool variable yet
byte cpu::RELe() {
    relative = bus.readBus(pc++);
    return relative;
}

// Reads LSB then MSB then combines them and shifts the MSB to the left
uint16_t cpu::ABSe() {
    absolute = bus.readBus(pc) | (bus.readBus(pc + 1) << 8);
    pc += 2;
    return absolute;
}

// Same as ABS except add x to the address
uint16_t cpu::ABSXe() {
    absolute = bus.readBus(pc) | (bus.readBus(pc + 1) << 8);
    pc += 2;
    absolute += x;
    return absolute;

}

// Same as ABS except add y to the address
uint16_t cpu::ABSYe() {
    absolute = bus.readBus(pc) | (bus.readBus(pc + 1) << 8);
    pc += 2;
    absolute += y;
    return absolute;

}

// Used just for JMP
uint16_t cpu::INDe() {
    uint16_t pointer = bus.readBus(pc) | (bus.readBus(pc + 1) << 8);
    pc += 2;
    uint16_t addressLow = bus.readBus(pointer);
    uint16_t addressHigh = bus.readBus((pointer & 0xFF00) | ((pointer + 1) & 0x00FF));
    jump = (addressHigh << 8) | addressLow;

    return jump;
}

uint16_t cpu::INDXe() {
    uint16_t indxetemp = bus.readBus(pc++);
    byte tL = (indxetemp + x) & 0xFF;
    byte tH = (indxetemp + x + 1) & 0xFF;

    byte indexL = bus.readBus(tL);
    byte indexH = bus.readBus(tH);


    return indexL | indexH << 8;

}

uint16_t cpu::INDYe() {
    uint16_t indxetemp = bus.readBus(pc++);
    byte tL = (indxetemp + y) & 0xFF;
    byte tH = (indxetemp + y + 1) & 0xFF;

    byte indexL = bus.readBus(tL);
    byte indexH = bus.readBus(tH);


    return indexL | indexH << 8;

}

// I wrote a switch statement for each opcode instead of using lookup table
void cpu::decodeAndExecute(byte instruction, int cycles, AddressingModes addressingMode) {
    byte operandValue = 0;
    uint16_t operandAddress = 0;
    if (addressingMode != ACC && addressingMode != IMP) {
        auto handlerIt = addressingModeHandlers.find(addressingMode);
        if (handlerIt != addressingModeHandlers.end()) {
            operandAddress = handlerIt->second();
            operandValue = bus.readBus(operandAddress);
        }
    }

    switch (instruction) {
        // Add With Carry
    case ADC:
        sum = a + operandValue + (status & flags::C ? 1 : 0);
        ADCresult = sum & 0xFF;
        overflow = ((a ^ operandValue) & 0x80) == 0 && ((a ^ ADCresult) & 0x80) != 0;

        status = (status & ~flags::V) | (overflow ? flags::V : 0);
        status = (status & ~flags::C) | (sum > 0xFF ? flags::C : 0);
        status = (a == 0) ? (status | flags::Z) : (status & ~flags::Z);
        status = (a & 0x80) ? (status | flags::N) : (status & ~flags::N);

        a = ADCresult;
        break;

        // Logical AND
    case AND:
        a = a & operandValue;
        status = (a == 0) ? (status | flags::Z) : (status & ~flags::Z);
        status = (a & 0x80) ? (status | flags::N) : (status & ~flags::N);
        break;

        // Arithmetic Shift Left
    case ASL:
        if (addressingMode == ACC) {
            status = (a & 0x80) ? (status | flags::C) : (status & ~flags::C);
            a = a << 1;
            status = (a == 0) ? (status | flags::Z) : (status & ~flags::Z);
            status = (a & 0x80) ? (status | flags::N) : (status & ~flags::N);
        }
        else {
            status = (operandValue & 0x80) ? (status | flags::C) : (status & ~flags::C);
            operandValue <<= 1;
            status = (operandValue == 0) ? (status | flags::Z) : (status & ~flags::Z);
            status = (operandValue & 0x80) ? (status | flags::N) : (status & ~flags::N);
            bus.writeBus(operandAddress, operandValue);
        }
        break;

        // Branch if Carry Clear
    case BCC:
        if (!(status & flags::C)) {
            int8_t signedOffset = static_cast<int8_t>(operandValue);
            pc += signedOffset;
        }
        break;

        // Branch if Carry Set
    case BCS:
        if (status & flags::C) {
            int8_t signedOffset = static_cast<int8_t>(operandValue);
            pc += signedOffset;
        }
        break;

        // Branch if Equal
    case BEQ:
        if (status & flags::Z) {
            int8_t signedOffset = static_cast<int8_t>(operandValue);
            pc += signedOffset;
        }

        // Bit Test
    case BIT:
        BITresult = a & operandValue;
        if (BITresult == 0) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        if (operandValue & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        if (operandValue & 0x40) {
            status |= flags::V;
        }
        else {
            status &= ~flags::V;
        }
        break;

        // Branch if Minuis
    case BMI:
        if (status & flags::N) {
            int8_t signedOffset = static_cast<int8_t>(operandValue);
            pc += signedOffset;
        }
        break;

        // Branch if Not Equal
    case BNE:
        if (!(status & flags::Z)) {
            int8_t signedOffset = static_cast<int8_t>(operandValue);
            pc += signedOffset;
        }
        break;

        // Branch if Positive
    case BPL:
        if (!(status & flags::N)) {
            int8_t signedOffset = static_cast<int8_t>(operandValue);
            pc += signedOffset;
        }
        break;

        // Force Interrupt
    case BRK:
        pc++;
        pushByteToStack((pc >> 8) & 0xFF);
        pushByteToStack(pc & 0xFF);
        pushByteToStack(status | flags::B);
        break;

        // Branch if Overflow Clear
    case BVC:
        if (!(status & flags::V)) {
            int8_t signedOffset = static_cast<int8_t>(operandValue);
            pc += signedOffset;
        }
        break;

        // Branch if Overflow Set
    case BVS:
        if (status & flags::V) {
            int8_t signedOffset = static_cast<int8_t>(operandValue);
            pc += signedOffset;
        }
        break;

        //Clear Carry Flag
    case CLC:
        status &= ~flags::C;
        break;

        // Clear Decimal Mode (Not used in NES)
    case CLD:
        status &= ~flags::D;
        break;

        // Clear Interrupt Disable
    case CLI:
        status &= ~flags::I;
        break;

        // Clear Overflow Flag
    case CLV:
        status &= ~flags::V;
        break;

        // Compare
    case CMP:
        if (a >= operandValue) {
            status |= flags::C;
        }
        else {
            status &= ~flags::C;
        }
        if (a == operandValue) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        CMPresult = a - operandValue;
        if (CMPresult & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Compare X Register
    case CPX:
        if (x >= operandValue) {
            status |= flags::C;
        }
        else {
            status &= ~flags::C;
        }
        if (x == operandValue) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        CPXresult = x - operandValue;
        if (CPXresult & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Compare Y Register
    case CPY:
        if (y >= operandValue) {
            status |= flags::C;
        }
        else {
            status &= ~flags::C;
        }
        if (y == operandValue) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        CPYresult = y - operandValue;
        if (CPYresult & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Decrement Memory
    case DEC:
        operandValue -= 1;
        bus.writeBus(operandAddress, operandValue);
        if (operandValue == 0) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        if (operandValue & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Decrement X Register
    case DEX:
        x -= 1;
        if (x == 0) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        if (x & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Decrement Y Register
    case DEY:
        y -= 1;
        if (y == 0) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        if (y & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Exclusive OR
    case EOR:
        a ^= operandValue;
        if (a == 0) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        if (a & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Increment Memory
    case INC:
        operandValue = (operandValue + 1) & 0xFF;
        bus.writeBus(operandAddress, operandValue);
        if (operandValue == 0) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        if (operandValue & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Increment X Register
    case INX:
        x = (x + 1) & 0xFF;
        if (x == 0) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        if (x & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Increment Y Register
    case INY:
        y = (y + 1) & 0xFF;
        if (y == 0) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        if (y & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Jump
    case JMP:
        pc = operandValue;
        break;

        // Jump to Subroutine
    case JSR:
        returnAddress = pc - 1;
        pushByteToStack((returnAddress >> 8) & 0xFF);
        pushByteToStack(returnAddress & 0xFF);
        pc = operandAddress;
        break;

        // Load Accumulator
    case LDA:
        a = operandValue;
        if (a == 0) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        if (a & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Load X Register
    case LDX:
        x = operandValue;
        if (x == 0) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        if (x & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Load Y Register
    case LDY:
        y = operandValue;
        if (y == 0) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        if (y & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Logical Shift Right
    case LSR:
        if (addressingMode == ACC) {
            status = (a & 0x01) ? (status | flags::C) : (status & ~flags::C);
            a = a >> 1;
            status = (a == 0) ? (status | flags::Z) : (status & ~flags::Z);
            status = (a & 0x80) ? (status | flags::N) : (status & ~flags::N);
        }
        else {
            status = (operandValue & 0x01) ? (status | flags::C) : (status & ~flags::C);
            operandValue = operandValue >> 1;
            status = (operandValue == 0) ? (status | flags::Z) : (status & ~flags::Z);
            status = (operandValue & 0x80) ? (status | flags::N) : (status & ~flags::N);
            bus.writeBus(operandAddress, operandValue);
        }
        break;

        // No Opeation
    case NOP:
        break;

        // Logical Inclusive OR
    case ORA:
        a = a | operandValue;
        if (a == 0) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        if (a & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Push Accumulator
    case PHA:
        pushByteToStack(a);
        break;

        // Push Processor Status
    case PHP:
        pushByteToStack(status | flags::B | flags::U);
        break;

        // Pull Accumulator
    case PLA:
        a = pop();
        if (a == 0) {
            status |= flags::Z;
        }
        else {
            status &= ~flags::Z;
        }
        if (a & 0x80) {
            status |= flags::N;
        }
        else {
            status &= ~flags::N;
        }
        break;

        // Pull Processor Status
    case PLP:
        newStatus = pop();
        newStatus = (newStatus & ~flags::B & ~flags::U) | (status & (flags::B | flags::U));
        status = newStatus;
        break;

        // Rotate Left
    case ROL:
        if (addressingMode == ACC) {
            int oldCarry = (status & flags::C) != 0 ? 1 : 0;
            status = (a & 0x80) ? (status | flags::C) : (status & ~flags::C);
            a = (a << 1) | oldCarry;
            status = (a == 0) ? (status | flags::Z) : (status & ~flags::Z);
            status = (a & 0x80) ? (status | flags::N) : (status & ~flags::N);
        }
        else {
            int oldCarry = (status & flags::C) != 0 ? 1 : 0;
            status = (operandValue & 0x80) ? (status | flags::C) : (status & ~flags::C);
            operandValue = (operandValue << 1) | oldCarry;
            status = (operandValue == 0) ? (status | flags::Z) : (status & ~flags::Z);
            status = (operandValue & 0x80) ? (status | flags::N) : (status & ~flags::N);
            bus.writeBus(operandAddress, operandValue);
        }
        break;

        // Rotate Right
    case ROR:
        if (addressingMode == ACC) {
            int oldCarry = (status & flags::C) != 0 ? 0x80 : 0;
            status = (a & 0x01) ? (status | flags::C) : (status & ~flags::C);
            a = (a >> 1) | oldCarry;
            status = (a == 0) ? (status | flags::Z) : (status & ~flags::Z);
            status = (a & 0x80) ? (status | flags::N) : (status & ~flags::N);
        }
        else {
            int oldCarry = (status & flags::C) != 0 ? 0x80 : 0;
            status = (operandValue & 0x01) ? (status | flags::C) : (status & ~flags::C);
            operandValue = (operandValue >> 1) | oldCarry;
            status = (operandValue == 0) ? (status | flags::Z) : (status & ~flags::Z);
            status = (operandValue & 0x80) ? (status | flags::N) : (status & ~flags::N);
            bus.writeBus(operandAddress, operandValue);
        }
        break;

        // Return from Interrupt
    case RTI:
        newStatusRTI = pop();
        // newStatusRTI = (newStatusRTI & ~flags::B & ~flags::U) | (status & (flags::B | flags::U)); IDK if i need this line
        status = newStatusRTI;
        break;
        RTIlow = pop();
        RTIhigh = pop();
        pc = (RTIhigh << 8) | RTIlow;

        // Return from Subroutine
    case RTS:
        RTSlow = pop();
        RTShigh = pop();
        pc = ((RTShigh << 8) | RTSlow) + 1;
        break;

        // Subtract with Carry
    case SBC:
        effectiveValue = operandValue + (status & flags::C ? 1 : 0);
        difference = a - effectiveValue;
        overflow = ((a ^ operandValue) & 0x80) && ((a ^ difference) & 0x80);

        status = (status & ~flags::V) | (overflow ? flags::V : 0);
        status = (status & ~flags::C) | (difference <= 0xFF ? flags::C : 0);
        status = ((difference & 0xFF) == 0) ? (status | flags::Z) : (status & ~flags::Z);
        status = (difference & 0x80) ? (status | flags::N) : (status & ~flags::N);

        a = difference & 0xFF;
        break;

        // Set Carry Flag
    case SEC:
        status = (status | flags::C);
        break;

        // Set Decimal Flag (Not needed for NES)
    case SED:
        status = (status | flags::D);
        break;

        // Set Interrupt Disable
    case SEI:
        status = (status | flags::I);
        break;

        // Store Accumulator 
    case STA:
        bus.writeBus(operandAddress, a);
        break;

        // Store X Register
    case STX:
        bus.writeBus(operandAddress, x);
        break;

        // Store Y Register
    case STY:
        bus.writeBus(operandAddress, y);
        break;

        // Transfer Accumulator to X
    case TAX:
        x = a;
        status = (x == 0) ? (status | flags::Z) : (status & ~flags::Z);
        status = (x & 0x80) ? (status | flags::N) : (status & ~flags::N);
        break;

        // Transfer Accumulator to Y
    case TAY:
        y = a;
        status = (y == 0) ? (status | flags::Z) : (status & ~flags::Z);
        status = (y & 0x80) ? (status | flags::N) : (status & ~flags::N);
        break;

        // Transfer Stack Pointer to X
    case TSX:
        x = stackpt;
        status = (x == 0) ? (status | flags::Z) : (status & ~flags::Z);
        status = (x & 0x80) ? (status | flags::N) : (status & ~flags::N);
        break;

        // Transfer X to Accumulator
    case TXA:
        a = x;
        status = (a == 0) ? (status | flags::Z) : (status & ~flags::Z);
        status = (a & 0x80) ? (status | flags::N) : (status & ~flags::N);
        break;

        // Transfer X to Stack Pointer
    case TXS:
        stackpt = x;
        break;

        // Transfer Y to Accumulator
    case TYA:
        a = y;
        status = (a == 0) ? (status | flags::Z) : (status & ~flags::Z);
        status = (a & 0x80) ? (status | flags::N) : (status & ~flags::N);
        break;
    }



}

byte cpu::pop() {
    stackpt++;
    return bus.readBus(stackpt + 0x100);
}

void cpu::pushByteToStack(byte value) {
    bus.writeBus(stackpt + 0x100, value);
    stackpt--;
}


void cpu::loadIntelHexFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line[0] != ':') continue; // Skip lines that do not start with ':'

        // Remove the start code ':' before processing
        line = line.substr(1);

        // Parse the byte count
        uint8_t byteCount = std::stoi(line.substr(0, 2), nullptr, 16);

        // Parse the address
        uint16_t address = std::stoi(line.substr(2, 4), nullptr, 16);

        // Parse the record type
        uint8_t recordType = std::stoi(line.substr(6, 2), nullptr, 16);

        if (recordType == 0x00) {
            for (int i = 0; i < byteCount; ++i) {
                uint8_t dataByte = std::stoi(line.substr(8 + i * 2, 2), nullptr, 16);
                bus.writeBus(address + i, dataByte);
                /* if (pc2 < 500) { For testing to make sure my parser works
                    std::cout << "Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(dataByte) << std::dec << std::endl;
                    pc2++;
                }
                */
            }
        }
        else if (recordType == 0x01) {
            break;
        }

    }
};


void cpu::loadToMemory(const std::vector<byte>& data, uint16_t startAddress) {
    for (size_t i = 0; i < data.size(); i++) {
        bus.writeBus(startAddress + i, data[i]);
    }
}