/*
CDA5155 2020 FALL Project 2, modified from Project 1

Name:Hsiang-Yuan Liao
UFID:4353-5341

On my honor, I have neither given nor received unauthorized aid on this assignment 
*/
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <map>
using namespace std;

#define STARTING_ADDRESS 256
#define BREAK_inst "01010100000000000000000000001101"
#define EXECUTE true
#define NO_EXECUTE false
#define win_endl "\r\n"

/* function declartions */
long convert2sComplement (string bin, int bits);
string disessamblyInst (string inst);
int executeInst (string inst, int PC);
void startDisessambly (void);
void startSimulation ();

/* global structures */
map<int, string> instMap;
map<int, int> dataMap;
int regMap[32] = {0};
int breakAddr = 0;
string newline = "\n";

int main (int argc, char* argv[]){

    /* Read input file and store all the instructins to hash map */
    ifstream inputFile (argv[1]);
    string line;
    int PC = STARTING_ADDRESS;

    if (inputFile.is_open()) {
        while (getline(inputFile, line)) {
            
            if (!line.empty() && line[line.size()-1] == '\r') {
                line.erase(line.size()-1);
                newline = win_endl;
            }

            instMap[PC] = line;
            PC += 4;
        }
        inputFile.close();
    } else {
        cout << "Unable to open " << argv[1] << endl;
        return 0;
    }

    /* Start output disassembly.txt */
    startDisessambly();
    
    /* Start output simulation.txt*/
    startSimulation();


    /* [DEBUG] print the instMap */
    /*
    map<int, int>::iterator it;
    for (it = dataMap.begin(); it != dataMap.end() ; it++) {
        cout << it->second << "\t" << it->first << "\n";
    }
    */
    /* end of DEBUG */
    return 0;
}

void startSimulation (void){
    map<int, int>::iterator it;
    int PC = STARTING_ADDRESS;
    int nextPC = STARTING_ADDRESS;
    
    ofstream outputFILE ("simulation.txt");
    if (!outputFILE.is_open()){
        cout << "Unable to open " << "disessembly.txt" << endl;
        return;
    }

    int cycles = 0;
    while (PC <= breakAddr){
        outputFILE << "--------------------" << newline; 
        outputFILE << "Cycle " << ++cycles << ":\t" << PC << "\t" << disessamblyInst(instMap[PC]) << newline << newline << "Registers";
        nextPC = executeInst(instMap[PC], PC);
        for (int i = 0 ; i < 32 ; i++){
			if (i % 8 == 0){
					outputFILE << newline;
                    outputFILE << "R" << setfill('0') << setw(2) << i << ":";
			}
            outputFILE << "\t" << regMap[i];
		}
        outputFILE << newline;
        outputFILE << newline <<"Data";
        int idx = 0;
        for (it = dataMap.begin() ; it != dataMap.end() ; it++){
            if (idx % 8 == 0){
                outputFILE << newline;
                outputFILE << setfill('0') << setw(3) << it->first << ":";
            }
            outputFILE << "\t" << it->second;
            idx++;
        }
        outputFILE << newline << newline;
        if (PC == nextPC)
            PC += 4;
        else
            PC = nextPC;
        
        //cout << "[DEBUG] PC = " << PC << endl;
    }
    outputFILE.close();
    return;
}

void startDisessambly (void){
    int PC = STARTING_ADDRESS, dataVal = 0;
    bool breakFound = false;
    map<int, string>::iterator it;

    ofstream outputFILE ("disessembly.txt");
    if (!outputFILE.is_open()){
        cout << "Unable to open " << "disessembly.txt" << endl;
        return;
    }

    while ((it = instMap.find(PC)) != instMap.end()){
        if (!breakFound){
            //cout << it->second << "\t" << it->first << "\t" << disessamblyInst(it->second) << endl;
            outputFILE << it->second << "\t" << it->first << "\t" << disessamblyInst(it->second) << newline;
            if (it->second == BREAK_inst){
                breakFound = true;
                breakAddr = it->first;
                //cout << "[DEBUG] Found BREAK inst at PC=" << it->first << endl;
            }
        } else {
            /*data memory part of input file*/
            //cout << it->second << "\t" << it->first << "\t"<< convert2sComplement(it->second) << endl;
            dataVal = convert2sComplement(it->second, 32);
            dataMap[PC] = dataVal;
            outputFILE << it->second << "\t" << it->first << "\t"<< dataVal << newline;
            
        }
        //cout << it->second << "\t" << it->first << "\n";
        PC += 4;
    }
    outputFILE.close();
    return;
}


long convert2sComplement (string bin, int bits){
    long ret = stol(bin, 0 , 2);
    // It's a positve number
    if((ret & (1 << (bits-1))) == 0)
        return ret;
    else {
        return ret | ~((1L << (bits-1)) -1L);
    }
    return 0;
}

enum inst_cat_op_code {
    eInstCatOne = 1,
    eInstCatTwo,
    eInstJ,
    eInstJR,
    eInstBEQ,
    eInstBLTZ,
    eInstBGTZ,
    eInstBREAK,
    eInstSW,
    eInstLW,
    eInstSLL,
    eInstSRL,
    eInstSRA,
    eInstNOP,
    eInstADD,
    eInstSUB,
    eInstMUL,
    eInstAND,
    eInstOR,
    eInstXOR,
    eInstNOR,
    eInstSLT,
    eInstADDI,
    eInstANDI,
    eInstORI,
    eInstXORI,
};

int instHash (string inst, int cat){
    if (!inst.compare("0000"))
        return (cat == eInstCatOne) ? eInstJ : eInstADD;
    else if(!inst.compare("0001"))
        return (cat == eInstCatOne) ? eInstJR : eInstSUB;
    else if(!inst.compare("0010"))
        return (cat == eInstCatOne) ? eInstBEQ : eInstMUL;
    else if(!inst.compare("0011"))
        return (cat == eInstCatOne) ? eInstBLTZ : eInstAND;
    else if(!inst.compare("0100"))
        return (cat == eInstCatOne) ? eInstBGTZ : eInstOR;
    else if(!inst.compare("0101"))
        return (cat == eInstCatOne) ? eInstBREAK : eInstXOR;
    else if(!inst.compare("0110"))
        return (cat == eInstCatOne) ? eInstSW : eInstNOR;
    else if(!inst.compare("0111"))
        return (cat == eInstCatOne) ? eInstLW : eInstSLT;
    else if(!inst.compare("1000"))
        return (cat == eInstCatOne) ? eInstSLL : eInstADDI;
    else if(!inst.compare("1001"))
        return (cat == eInstCatOne) ? eInstSRL : eInstANDI;
    else if(!inst.compare("1010"))
        return (cat == eInstCatOne) ? eInstSRA : eInstORI;
    else if(!inst.compare("1011"))
        return (cat == eInstCatOne) ? eInstNOP : eInstXORI;
    else
        return 0;
}

string addressOffsetSigned(string inst, int start, int shift){
    return to_string((int) convert2sComplement(inst.substr(start), 16) << shift);
}

string addressOffset(string inst, int start, int shift){
    return to_string(stoi(inst.substr(start), 0, 2) << shift);
}

string registerOffset(string inst, int start, int len) {
    return to_string(stoi(inst.substr(start ,len), 0, 2));
}

int addrOffsetInt(string inst, int start, int shift){
    return stoi(inst.substr(start), 0, 2) << shift;
}

int regOffsetInt(string inst, int start, int len){
    return stoi(inst.substr(start ,len), 0, 2);
}

string disessamblyInst (string inst){
    string ret = "";
    string instCat = inst.substr(0,2);
    string instOp = inst.substr(2,4);
    string rs = "R" + registerOffset(inst, 6, 5);
    string rt = "R" + registerOffset(inst, 11, 5);
    string rd = "R" + registerOffset(inst, 16, 5);

    int category = (instCat.compare("01") == 0) ? eInstCatOne : eInstCatTwo;
    switch (category){
        case eInstCatOne: /* Category-1 */
            switch (instHash(instOp, eInstCatOne)){
                case eInstJ:
                    ret += "J #" + addressOffset(inst, 6, 2);
                    break;
                case eInstJR:
                    ret += "JR " + rs;
                    break;
                case eInstBEQ:
                    ret += "BEQ " + rs + ", " + rt + ", " + "#" + addressOffset(inst, 16, 2);
                    break;
                case eInstBLTZ:
                    ret += "BLTZ " + rs + ", " + "#" + addressOffset(inst, 16, 2);
                    break;
                case eInstBGTZ:
                    ret += "BGTZ " + rs + ", " + "#" + addressOffset(inst, 16, 2);
                    break;
                case eInstBREAK:
                    ret += "BREAK";
                    break;
                case eInstSW:
                    ret += "SW " + rt + ", " + addressOffset(inst, 16, 0) + "(" + rs + ")";
                    break;
                case eInstLW:
                    ret += "LW " + rt + ", " + addressOffset(inst, 16, 0) + "(" + rs + ")";
                    break;
                case eInstSLL:
                    ret += "SLL R" + registerOffset(inst, 16, 5) + ", " + rt + ", " + "#" + registerOffset(inst, 21, 5);
                    break;
                case eInstSRL:
                    ret += "SRL R" + registerOffset(inst, 16, 5) + ", " + rt + ", " + "#" + registerOffset(inst, 21, 5);
                    break;
                case eInstSRA:
                    ret += "SRA R" + registerOffset(inst, 16, 5) + ", " + rt + ", " + "#" + registerOffset(inst, 21, 5);
                    break;
                case eInstNOP:
                    ret += "NOP";
                    break;                                                           
            }
        break;
        case eInstCatTwo:
            switch (instHash(instOp, eInstCatTwo)){
                case eInstADD: 
                    ret += "ADD " + rd + ", "  + rs + ", " + rt;
                    break;
                case eInstSUB:
                    ret += "SUB " + rd + ", "  + rs + ", " + rt;
                    break;
                case eInstMUL:
                    ret += "MUL " + rd + ", "  + rs + ", " + rt;
                    break;
                case eInstAND:
                    ret += "AND " + rd + ", "  + rs + ", " + rt;
                    break;
                case eInstOR:
                    ret += "OR " + rd + ", "  + rs + ", " + rt;
                    break;
                case eInstXOR:
                    ret += "XOR " + rd + ", "  + rs + ", " + rt;
                    break;
                case eInstNOR:
                    ret += "NOR " + rd + ", "  + rs + ", " + rt;
                    break;
                case eInstSLT:
                    ret += "SLT " + rd + ", "  + rs + ", " + rt;
                    break;
                case eInstADDI:
                    ret += "ADDI " + rt + ", "  + rs + ", " + "#" + addressOffsetSigned(inst, 16, 0);
                    break;
                case eInstANDI:
                    ret += "ANDI " + rt + ", "  + rs + ", " + "#" + addressOffset(inst, 16, 0);
                    break;
                case eInstORI:
                    ret += "ORI " + rt + ", "  + rs + ", " + "#" + addressOffset(inst, 16, 0);
                    break;
                case eInstXORI:
                    ret += "XORI " + rt + ", "  + rs + ", " + "#" + addressOffset(inst, 16, 0);
                    break;
            }
        break;
    }

    return ret;
}

int executeInst (string inst, int PC){
    int ret = PC;
    unsigned int tmp;
    string instCat = inst.substr(0,2);
    string instOp = inst.substr(2,4);
    int rsI = regOffsetInt(inst, 6, 5);
    int rtI = regOffsetInt(inst, 11, 5);
    int rdI = regOffsetInt(inst, 16, 5);


    int category = (instCat.compare("01") == 0) ? eInstCatOne : eInstCatTwo;
    switch (category){
        case eInstCatOne: /* Category-1 */
            switch (instHash(instOp, eInstCatOne)){
                case eInstJ: 
                    ret = addrOffsetInt(inst, 6, 2);
                    break;
                case eInstJR:
                    ret= regMap[rsI];
                    break;
                case eInstBEQ:
                    if (regMap[rsI] == regMap[rtI])
                        ret = PC + 4 + addrOffsetInt(inst, 16, 2);
                    break;
                case eInstBLTZ:
                    if (regMap[rsI] < 0)
                        ret = PC + 4 + addrOffsetInt(inst, 16, 2);
                    break;
                case eInstBGTZ:
                    if (regMap[rsI] > 0)
                        ret = PC + 4 + addrOffsetInt(inst, 16, 2);
                    break;
                case eInstBREAK:
                    break;
                case eInstSW:
                    dataMap[regMap[rsI] + addrOffsetInt(inst, 16, 0)] = regMap[rtI];
                    break;
                case eInstLW:
                    regMap[rtI] = dataMap[regMap[rsI] + addrOffsetInt(inst, 16, 0)];
                    break;
                case eInstSLL:
                    regMap[regOffsetInt(inst, 16, 5)] = regMap[rtI] << regOffsetInt(inst, 21, 5);
                    break;
                case eInstSRL:
                    tmp = (unsigned int) regMap[rtI] >> regOffsetInt(inst, 21, 5);
                    regMap[regOffsetInt(inst, 16, 5)] = (int) tmp;
                    break;
                case eInstSRA:
                    regMap[regOffsetInt(inst, 16, 5)] = regMap[rtI] >> regOffsetInt(inst, 21, 5);
                    break;
                case eInstNOP: /* SLL R0 R0 0*/
                    break;                                                           
            }
        break;
        
        case eInstCatTwo:
            switch (instHash(instOp, eInstCatTwo)){
                case eInstADD:
                    regMap[rdI] = regMap[rsI] + regMap[rtI];
                    break;
                case eInstSUB:
                    regMap[rdI] = regMap[rsI] - regMap[rtI];
                    break;
                case eInstMUL:
                    regMap[rdI] = regMap[rsI] * regMap[rtI];
                    break;
                case eInstAND:
                    regMap[rdI] = regMap[rsI] & regMap[rtI];
                    break;
                case eInstOR:
                    regMap[rdI] = regMap[rsI] | regMap[rtI];
                    break;
                case eInstXOR:
                    regMap[rdI] = regMap[rsI] ^ regMap[rtI];
                    break;
                case eInstNOR:
                    regMap[rdI] = ~(regMap[rsI] | regMap[rtI]);
                    break;
                case eInstSLT:
                    if (regMap[rsI] < regMap[rtI])
                        regMap[rdI] = 1;
                    else
                        regMap[rdI] = 0;
                    break;
                case eInstADDI:
                    regMap[rtI] = regMap[rsI] + convert2sComplement(inst.substr(16), 16);
                    break;
                case eInstANDI:
                    regMap[rtI] = regMap[rsI] & addrOffsetInt(inst, 16, 0);
                    break;
                case eInstORI:
                    regMap[rtI] = regMap[rsI] | addrOffsetInt(inst, 16, 0);
                    break;
                case eInstXORI:
                    regMap[rtI] = regMap[rsI] ^ addrOffsetInt(inst, 16, 0);
                    break;
            }
            
        break;
    }

    return ret;
}