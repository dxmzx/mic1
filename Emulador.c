#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Types
typedef unsigned char byte;      // 8 bits
typedef unsigned int word;       // 32 bits
typedef unsigned long microinstr; // 64 bits (using only 36 bits)

// Constants
#define MEMORY_SIZE 100000000
#define PROGRAM_START_ADDRESS 0x0401
#define MICROPROGRAM_SIZE 512

// Memory
byte *memory;

// Control Storage
microinstr control_store[MICROPROGRAM_SIZE];

// Registers
word MAR = 0, MDR = 0, PC = 0;
byte MBR = 0;
word SP = 0, LV = 0, TOS = 0, OPC = 0, CPP = 0, H = 0;
microinstr MIR;
word MPC = 0;

// Buses
word BusB, BusC;

// Flags
byte N = 0, Z = 0;

// Microinstruction fields
byte MIR_B, MIR_OP, MIR_Shift, MIR_Mem, MIR_Jump;
word MIR_C;

// Function Prototypes
void init_memory();
void free_memory();
void load_microprogram();
void load_program(const char *program);
void fetch_decode_execute_cycle();

void decode_microinstr();
void assign_bus_B();
void alu();
void assign_bus_C();
void memory_operation();
void jump();

word read_word(word address);
void write_word(word address, word value);
byte read_byte(word address);
void write_byte(word address, byte value);

// Debug
void print_binary(void *value, int type);
void show_state();


// ===================== Main =====================
int main(int argc, const char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s program_file\n", argv[0]);
        return 1;
    }

    init_memory();
    load_microprogram();
    load_program(argv[1]);

    while (1) {
        show_state();
        fetch_decode_execute_cycle();
    }

    free_memory();
    return 0;
}


// ================= Memory Management =================

void init_memory() {
    memory = (byte *)malloc(MEMORY_SIZE);
    if (memory == NULL) {
        printf("Memory allocation failed!\n");
        exit(1);
    }
    memset(memory, 0, MEMORY_SIZE);
}

void free_memory() {
    free(memory);
}

word read_word(word address) {
    if (address * 4 + 3 >= MEMORY_SIZE) {
        printf("Memory read error at address %X\n", address);
        exit(1);
    }
    word value;
    memcpy(&value, &memory[address * 4], 4);
    return value;
}

void write_word(word address, word value) {
    if (address * 4 + 3 >= MEMORY_SIZE) {
        printf("Memory write error at address %X\n", address);
        exit(1);
    }
    memcpy(&memory[address * 4], &value, 4);
}

byte read_byte(word address) {
    if (address >= MEMORY_SIZE) {
        printf("Memory read error at address %X\n", address);
        exit(1);
    }
    return memory[address];
}

void write_byte(word address, byte value) {
    if (address >= MEMORY_SIZE) {
        printf("Memory write error at address %X\n", address);
        exit(1);
    }
    memory[address] = value;
}


// ================= Load Programs =================

void load_microprogram() {
    FILE *file = fopen("microprog.rom", "rb");
    if (!file) {
        printf("Error opening microprogram file.\n");
        exit(1);
    }
    fread(control_store, sizeof(microinstr), MICROPROGRAM_SIZE, file);
    fclose(file);
}

void load_program(const char *program) {
    FILE *file = fopen(program, "rb");
    if (!file) {
        printf("Error opening program file.\n");
        exit(1);
    }

    word size;
    byte size_bytes[4];

    fread(size_bytes, sizeof(byte), 4, file);
    memcpy(&size, size_bytes, 4);

    fread(memory, sizeof(byte), 20, file);
    fread(&memory[PROGRAM_START_ADDRESS], sizeof(byte), size - 20, file);

    fclose(file);
}


// ================= Fetch-Decode-Execute =================

void fetch_decode_execute_cycle() {
    MIR = control_store[MPC];

    decode_microinstr();
    assign_bus_B();
    alu();
    assign_bus_C();
    memory_operation();
    jump();
}


// ================= Microinstruction Decode =================

void decode_microinstr() {
    MIR_B        = (MIR) & 0b1111;
    MIR_Mem      = (MIR >> 4) & 0b111;
    MIR_C        = (MIR >> 7) & 0b111111111;
    MIR_OP       = (MIR >> 16) & 0b111111;
    MIR_Shift    = (MIR >> 22) & 0b11;
    MIR_Jump     = (MIR >> 24) & 0b111;
    MPC          = (MIR >> 27) & 0b111111111;
}


// ================= Bus Operations =================

void assign_bus_B() {
    switch (MIR_B) {
        case 0: BusB = MDR; break;
        case 1: BusB = PC; break;
        case 2: // MBR with sign extension
            BusB = (MBR & 0x80) ? (MBR | 0xFFFFFF00) : MBR;
            break;
        case 3: BusB = MBR; break;
        case 4: BusB = SP; break;
        case 5: BusB = LV; break;
        case 6: BusB = CPP; break;
        case 7: BusB = TOS; break;
        case 8: BusB = OPC; break;
        default: BusB = 0xFFFFFFFF; break;
    }
}


// ================= ALU =================

void alu() {
    switch (MIR_OP) {
        case 12: BusC = H & BusB; break;
        case 17: BusC = 1; break;
        case 18: BusC = -1; break;
        case 20: BusC = BusB; break;
        case 24: BusC = H; break;
        case 26: BusC = ~H; break;
        case 28: BusC = H | BusB; break;
        case 44: BusC = ~BusB; break;
        case 53: BusC = BusB + 1; break;
        case 54: BusC = BusB - 1; break;
        case 57: BusC = H + 1; break;
        case 59: BusC = -H; break;
        case 60: BusC = H + BusB; break;
        case 61: BusC = H + BusB + 1; break;
        case 63: BusC = BusB - H; break;
        default: BusC = 0; break;
    }

    // Flags
    Z = (BusC == 0);
    N = (BusC != 0);

    // Shift
    switch (MIR_Shift) {
        case 1: BusC = BusC << 8; break;
        case 2: BusC = BusC >> 1; break;
    }
}


// ================= Assign Bus C =================

void assign_bus_C() {
    if (MIR_C & 0b000000001) MAR = BusC;
    if (MIR_C & 0b000000010) MDR = BusC;
    if (MIR_C & 0b000000100) PC  = BusC;
    if (MIR_C & 0b000001000) SP  = BusC;
    if (MIR_C & 0b000010000) LV  = BusC;
    if (MIR_C & 0b000100000) CPP = BusC;
    if (MIR_C & 0b001000000) TOS = BusC;
    if (MIR_C & 0b010000000) OPC = BusC;
    if (MIR_C & 0b100000000) H   = BusC;
}


// ================= Memory Operation =================

void memory_operation() {
    if (MIR_Mem & 0b001) MBR = read_byte(PC);
    if (MIR_Mem & 0b010) MDR = read_word(MAR);
    if (MIR_Mem & 0b100) write_word(MAR, MDR);
}


// ================= Jump Logic =================

void jump() {
    if (MIR_Jump & 0b001) MPC |= (N << 8);
    if (MIR_Jump & 0b010) MPC |= (Z << 8);
    if (MIR_Jump & 0b100) MPC |= MBR;
}

}
