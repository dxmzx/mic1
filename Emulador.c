#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Tipos
typedef unsigned char byte;       // 8 bits
typedef unsigned int word;        // 32 bits
typedef unsigned long int microinstr; // 64 bits (usado 36 bits)

// Registradores
word MAR = 0, MDR = 0, PC = 0;    // Memória
byte MBR = 0;                     // Memória

word SP = 0, LV = 0, TOS = 0,     // Operações da ULA
     OPC = 0, CPP = 0, H = 0;     // Operações da ULA

microinstr MIR;                   // Microinstrução atual
word MPC = 0;                     // Próxima microinstrução

// Barramentos
word bus_B, bus_C;

// Flip-Flops
byte N, Z;

// Auxiladores para Decodificar Microinstrução
byte MIR_B, MIR_operation, MIR_shifter, MIR_mem, MIR_jump;
word MIR_C;

// Armazenamento de Controle
#define CONTROL_STORE_SIZE 512
microinstr control_store[CONTROL_STORE_SIZE];

// Memória principal
#define MEMORY_SIZE 100000000
byte memory[MEMORY_SIZE];

// Prototipo das Funções
void load_control_store();
void load_program(const char *program_file);
void show_status();
void decode_microinstruction();
void assign_bus_B();
void alu_operation();
void assign_bus_C();
void memory_operation();
void jump_control();
void print_binary(void* value, int type);

// Laço Principal
int main(int argc, const char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <arquivo_programa>\n", argv[0]);
        return 1;
    }

    load_control_store();
    load_program(argv[1]);

    while (1) {
        show_status();
        MIR = control_store[MPC];

        decode_microinstruction();
        assign_bus_B();
        alu_operation();
        assign_bus_C();
        memory_operation();
        jump_control();
    }

    return 0;
}

// Implementação das Funções

void load_control_store() {
    FILE *file = fopen("microprog.rom", "rb");
    if (!file) {
        perror("Erro ao abrir microprog.rom");
        exit(1);
    }

    fread(control_store, sizeof(microinstr), CONTROL_STORE_SIZE, file);
    fclose(file);
}

void load_program(const char* program_file) {
    FILE *file = fopen(program_file, "rb");
    if (!file) {
        perror("Erro ao abrir programa");
        exit(1);
    }

    word size;
    fread(&size, sizeof(byte), 4, file);
    fread(memory, sizeof(byte), 20, file);
    fread(&memory[0x0401], sizeof(byte), size - 20, file);

    fclose(file);
}

void decode_microinstruction() {
    MIR_B = MIR & 0b1111;
    MIR_mem = (MIR >> 4) & 0b111;
    MIR_C = (MIR >> 7) & 0b111111111;
    MIR_operation = (MIR >> 16) & 0b111111;
    MIR_shifter = (MIR >> 22) & 0b11;
    MIR_jump = (MIR >> 24) & 0b111;
    MPC = (MIR >> 27) & 0b111111111;
}

void assign_bus_B() {
    switch (MIR_B) {
        case 0: bus_B = MDR; break;
        case 1: bus_B = PC; break;
        case 2: 
            bus_B = (MBR & 0x80) ? (MBR | 0xFFFFFF00) : MBR;
            break;
        case 3: bus_B = MBR; break;
        case 4: bus_B = SP; break;
        case 5: bus_B = LV; break;
        case 6: bus_B = CPP; break;
        case 7: bus_B = TOS; break;
        case 8: bus_B = OPC; break;
        default: bus_B = 0xFFFFFFFF; break;
    }
}

void alu_operation() {
    switch (MIR_operation) {
        case 12: bus_C = H & bus_B; break;
        case 17: bus_C = 1; break;
        case 18: bus_C = -1; break;
        case 20: bus_C = bus_B; break;
        case 24: bus_C = H; break;
        case 26: bus_C = ~H; break;
        case 28: bus_C = H | bus_B; break;
        case 44: bus_C = ~bus_B; break;
        case 53: bus_C = bus_B + 1; break;
        case 54: bus_C = bus_B - 1; break;
        case 57: bus_C = H + 1; break;
        case 59: bus_C = -H; break;
        case 60: bus_C = H + bus_B; break;
        case 61: bus_C = H + bus_B + 1; break;
        case 63: bus_C = bus_B - H; break;
        default: bus_C = 0; break;
    }

    N = (bus_C != 0) ? 0 : 1;
    Z = (bus_C == 0) ? 1 : 0;

    switch (MIR_shifter) {
        case 1: bus_C <<= 8; break;
        case 2: bus_C >>= 1; break;
    }
}

void assign_bus_C() {
    if (MIR_C & 0b000000001) MAR = bus_C;
    if (MIR_C & 0b000000010) MDR = bus_C;
    if (MIR_C & 0b000000100) PC  = bus_C;
    if (MIR_C & 0b000001000) SP  = bus_C;
    if (MIR_C & 0b000010000) LV  = bus_C;
    if (MIR_C & 0b000100000) CPP = bus_C;
    if (MIR_C & 0b001000000) TOS = bus_C;
    if (MIR_C & 0b010000000) OPC = bus_C;
    if (MIR_C & 0b100000000) H   = bus_C;
}

void memory_operation() {
    if (MIR_mem & 0b001) MBR = memory[PC];
    if (MIR_mem & 0b010) memcpy(&MDR, &memory[MAR * 4], 4);
    if (MIR_mem & 0b100) memcpy(&memory[MAR * 4], &MDR, 4);
}

void jump_control() {
    if (MIR_jump & 0b001) MPC |= (N << 8);
    if (MIR_jump & 0b010) MPC |= (Z << 8);
    if (MIR_jump & 0b100) MPC |= MBR;
}

void show_status() {
    printf("\n==================== ESTADO DO SISTEMA ====================\n");

    printf("\nREGISTRADORES:\n");
    printf("MAR: "); print_binary(&MAR, 3); printf(" (%X)\n", MAR);
    printf("MDR: "); print_binary(&MDR, 3); printf(" (%X)\n", MDR);
    printf("PC : "); print_binary(&PC , 3); printf(" (%X)\n", PC);
    printf("MBR: "); print_binary(&MBR, 2); printf(" (%X)\n", MBR);
    printf("SP : "); print_binary(&SP , 3); printf(" (%X)\n", SP);
    printf("LV : "); print_binary(&LV , 3); printf(" (%X)\n", LV);
    printf("CPP: "); print_binary(&CPP, 3); printf(" (%X)\n", CPP);
    printf("TOS: "); print_binary(&TOS, 3); printf(" (%X)\n", TOS);
    printf("OPC: "); print_binary(&OPC, 3); printf(" (%X)\n", OPC);
    printf("H  : "); print_binary(&H  , 3); printf(" (%X)\n", H);
    printf("MPC: "); print_binary(&MPC, 5); printf(" (%X)\n", MPC);
    printf("MIR: "); print_binary(&MIR, 4); printf("\n");

    getchar();
}

//FUNÇÃO RESPONSAVEL POR PRINTAR OS VALORES EM BINARIO
//TIPO 1: Imprime o binário de 4 bytes seguidos 
//TIPO 2: Imprime o binário de 1 byte
//TIPO 3: Imprime	o binario de uma palavra 
//TIPO 4: Imprime o binário de uma microinstrução
//TIPO 5: Imprime o binário dos 9 bits do MPC

void print_binary(void* value, int type) {
    switch (type) {
        case 1: { // 4 bytes
            byte* v = (byte*)value;
            for (int i = 3; i >= 0; i--) {
                for (int j = 7; j >= 0; j--)
                    printf("%d", (v[i] >> j) & 1);
                printf(" ");
            }
            break;
        }
        case 2: { // 1 byte
            byte v = *((byte*)value);
            for (int j = 7; j >= 0; j--)
                printf("%d", (v >> j) & 1);
            break;
        }
        case 3: { // word
            word v = *((word*)value);
            for (int j = 31; j >= 0; j--)
                printf("%d", (v >> j) & 1);
            break;
        }
        case 4: { // microinstr (36 bits)
            microinstr v = *((microinstr*)value);
            for (int j = 35; j >= 0; j--) {
                printf("%ld", (v >> j) & 1);
                if (j == 32 || j == 29 || j == 20 || j == 12 || j == 9) printf(" ");
            }
            break;
        }
        case 5: { // 9 bits do MPC
            word v = *((word*)value) & 0x1FF;
            for (int j = 8; j >= 0; j--)
                printf("%d", (v >> j) & 1);
            break;
        }
    }
}
