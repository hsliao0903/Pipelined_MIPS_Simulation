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
#include <queue>
#include <set>
using namespace std;

#define STARTING_ADDRESS 256
#define BREAK_inst "01010100000000000000000000001101"
#define EXECUTE true
#define NO_EXECUTE false
#define win_endl "\r\n"

/* Pipeline definations */
#define MAX_FETCH 2  // the maximum number of instructions could be fetched per cycle
#define PREISSUEQ_SIZE 4
#define PREALU1Q_SIZE 2
#define PREALU2Q_SIZE 2
#define WRITEBACKQ_SIZE 2

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

/* function declartions */
long convert2sComplement (string bin, int bits);
string disessamblyInst (string inst);
int executeInst (string inst, int PC);
void startDisessambly (void);
void startSimulation ();
int instHash (string inst, int cat);

/* pipeline use function declartions */
void startPipelineSimulation (void);
void outputSimulation (ofstream& outputFILE, int* cycleCounts);
int getInstCode (string inst);
bool isBranchInst (string inst);
bool tryIssueInst (string inst);
bool isRAW (int reg);
bool isWAW (int reg);
bool isWAR (int reg);
bool isPreIssueRAW (int reg);
bool isPreIssueWAW (int reg);
bool isPreIssueWAR (int reg);
bool checkNonIssuedInst (string inst, bool check);
bool checkBranchInst (string inst);


/* global structures */
map<int, string> instMap;
map<int, int> dataMap;
int regMap[32] = {0};
int breakAddr = 0;
string newline = "\n";

// scoreboarding for hazard checks
set<int> sbRegReading;
set<int> sbRegWriting;
set<int> preissueRegRead;  // for the order property in pre-issued queue, (not-issued istructions)
set<int> preissueRegWrite;
//IF unit use
string IFWaitInst = "";
string IFExecIns = "";
// Issue unit
queue<string> preIssueQ; // max size 4
//queue<string> swOrderQ; 
queue<string> preALU1Q; // max size 2
queue<string> preALU2Q; // max size 2
queue<string> writeBackQ; //max size 2
string preMEMQ = "";
string postMEMQ = "";
string postALU2Q = "";




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
            if (line == BREAK_inst){
                breakAddr = PC;
                //cout << "[DEBUG] Found BREAK inst at PC=" << PC << endl;
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
    //startSimulation();

    /* Start Pipeline Simulation*/
    startPipelineSimulation();


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

void startPipelineSimulation (void){
    int nextPC = STARTING_ADDRESS;
    int PC = STARTING_ADDRESS;
    int cycleCount = 0;
    // fetch parameters initialize
    //bool isIFwaiting = false;  //wait for a branch to finish
    int fetchCount = 0; //at most 2
    bool isPreIssueQFull = false;
    bool isPreALU1QFull = false;
    bool isPreALU2QFull = false;
    string fetchedInst = "";


    ofstream outputFILE ("simulation.txt");
    if (!outputFILE.is_open()){
        cout << "Unable to open " << "simulation.txt" << endl;
        return;
    }

    while (nextPC < breakAddr){
        cycleCount += 1;
        isPreIssueQFull = false;
        isPreALU1QFull = false;
        isPreALU2QFull = false;

        /* POSTMEM queue to write back */
        if (!postMEMQ.empty()){
            writeBackQ.push(postMEMQ);
            postMEMQ = "";
        }

        /* MEM UNIT */
        if (!preMEMQ.empty()){
                // for LW
            if (getInstCode(preMEMQ) == eInstLW){
                postMEMQ = preMEMQ;
                preMEMQ = "";
            } else if (getInstCode(preMEMQ) == eInstSW){
                // for SW
                writeBackQ.push(preMEMQ);
                preMEMQ = "";
            } else {
                cout << "[ERROR] something wrong with PRE-MEM queue" << newline;
                break;
            }
            
        }

        /* postALU2 to WB */
        if (!postALU2Q.empty()){
            writeBackQ.push(postALU2Q);
            postALU2Q = "";
        }
        

        /* ALU1 UNIT */
        if (!preALU1Q.empty()){
            int qSize = preALU1Q.size();
            if (qSize == PREALU1Q_SIZE)
                isPreALU1QFull = true;
            preMEMQ = preALU1Q.front();
            preALU1Q.pop();
            
        }

        /* ALU2 UNIT */
        if (!preALU2Q.empty()){
            int qSize = preALU2Q.size();
            if (qSize == PREALU2Q_SIZE)
                isPreALU2QFull = true;
            postALU2Q = preALU2Q.front();
            preALU2Q.pop();
        }
        
        /* ISSUE UNIT */
        preissueRegRead.clear();
        preissueRegWrite.clear();
        int preissueSpaceLeftLastCycle = PREISSUEQ_SIZE;
        if (!preIssueQ.empty()){
            // check all the instructions in the queue
            int qSize = preIssueQ.size();
            int iterQCount = 0;
            // at most one inst to ALU1 and one inst to ALU2
            bool issuedALU1 = false;
            bool issuedALU2 = false;
            bool swFound = false;
            int swPreIssueOrder = PREISSUEQ_SIZE + 1;
            // preissueQ is full last cycle
            if (qSize == PREISSUEQ_SIZE)
                isPreIssueQFull = true;
            else
                preissueSpaceLeftLastCycle = PREISSUEQ_SIZE - qSize;
            


            while (iterQCount < qSize){
                string curInst = preIssueQ.front(); // the instrion dealing with
                //preIssueQ.pop();
                
                if (getInstCode(curInst) == eInstLW){
                    // for ALU1 LW instructions 
                    if (iterQCount < swPreIssueOrder && !issuedALU1 && !isPreALU1QFull && checkNonIssuedInst(curInst, true)){
                        // deal with LW instruction //check hazard passed
                        // check hazard with non-issued instructions
                        if (tryIssueInst(curInst)){
                            checkNonIssuedInst(curInst, false);
                            preIssueQ.pop();
                            preALU1Q.push(curInst);
                            issuedALU1 = true;
                        } else {
                            checkNonIssuedInst(curInst, false);
                            preIssueQ.push(curInst);
                            preIssueQ.pop();
                        }
                    } else {
                        checkNonIssuedInst(curInst, false);
                        preIssueQ.push(curInst);
                        preIssueQ.pop();
                    }
                    
                } else if (getInstCode(curInst) == eInstSW){
                    // remember the first SW instruction's order in pre-issue queue
                    if (!swFound){
                        swFound = true;
                        swPreIssueOrder = iterQCount;
                    }
                    //for ALU1 instructions SW
                    if (!issuedALU1 && !isPreALU1QFull && checkNonIssuedInst(curInst, true)){
                        // deal with SW instruction
                        if (tryIssueInst(curInst)){
                            checkNonIssuedInst(curInst, false);
                            preIssueQ.pop();
                            preALU1Q.push(curInst);
                            //swOrderQ.pop();
                            issuedALU1 = true;
                        } else {
                            checkNonIssuedInst(curInst, false);
                            preIssueQ.push(curInst);
                            preIssueQ.pop();
                        }
                    } else {
                        checkNonIssuedInst(curInst, false);
                        preIssueQ.push(curInst);
                        preIssueQ.pop();
                    }
                    
                } else {
                    // for ALU2 instructions
                    if (!issuedALU2 && !isPreALU2QFull && checkNonIssuedInst(curInst, true)){
                        // deal with it
                        if (tryIssueInst(curInst)){
                            checkNonIssuedInst(curInst, false);
                            preIssueQ.pop();
                            preALU2Q.push(curInst);
                            issuedALU2 = true;
                        } else {
                            checkNonIssuedInst(curInst, false);
                            preIssueQ.push(curInst);
                            preIssueQ.pop();
                        }
                    } else {
                        checkNonIssuedInst(curInst, false);
                        preIssueQ.push(curInst);
                        preIssueQ.pop();
                    }
                }
                iterQCount++;
            }
        }
        
        /* Deal with Branch Exec queue*/
        if (!IFExecIns.empty()){
            IFWaitInst = "";
            IFExecIns = "";
        }

        /* Deal with Branch Instructions */
        if (!IFWaitInst.empty()){
            if (checkBranchInst(IFWaitInst)){
                IFExecIns = IFWaitInst;
                IFWaitInst = "lastwaitcycle";
                nextPC = executeInst(IFExecIns, PC);
                cout << "[DEBUG] nextPC = " << nextPC << endl; 
            }
        }

        /* FETCH UNIT */
        // 2 conditions, no branch inst waiting , preissue queue is not full, fetch maximum 2 instructions
        fetchCount = 0;
        while (IFWaitInst.empty() && fetchCount < preissueSpaceLeftLastCycle && fetchCount < MAX_FETCH && !isPreIssueQFull && preIssueQ.size() < PREISSUEQ_SIZE){
            PC = nextPC;
            fetchedInst = instMap[PC];

            // fetch a BREAK instruction
            if (fetchedInst == BREAK_inst){
                IFExecIns = fetchedInst;
                IFWaitInst = "";
                nextPC = breakAddr; // to the end
                break;
            }

            // TODO: fetch a NOP instruction
            if (getInstCode(fetchedInst) == eInstNOP){
                cout << "get a NOP instruction" << endl;
                IFExecIns = fetchedInst;
                IFWaitInst = "";
                nextPC += 4;
                fetchCount += 1;
                continue;
            }

            // fetch a BRANCH instruction
            if (isBranchInst(fetchedInst)){
                
                //IFWaitInst = fetchedInst;
                //IFExecIns = "";

                // check if it is a jump immediate instruction
                if (getInstCode(fetchedInst) == eInstJ){
                    cout << "get a jump bransch Cycle: " << cycleCount << " fetchcount: " << fetchCount << endl;
                    nextPC = executeInst(fetchedInst, PC);
                    cout << "nextPC: " << nextPC << endl;
                    IFExecIns = fetchedInst;
                    IFWaitInst = "";
                    break;
                } else {
                    // check if these branches has data hazards, if yes, wait, if no, do as jump does
                    if (checkBranchInst(fetchedInst)){
                        nextPC = executeInst(fetchedInst, PC);
                        IFExecIns = fetchedInst;
                        IFWaitInst = "";
                        break;
                    } else {
                        IFWaitInst = fetchedInst;
                        IFExecIns = "";
                        break;
                    }
                }

                //break;
            }

            // remember the SW instruction order for ISSUE unit use
            //if (getInstCode(fetchedInst) == eInstSW){
            //    swOrderQ.push(fetchedInst);
            //}

            preIssueQ.push(fetchedInst);
            fetchCount += 1;
            nextPC += 4;
        }
        
        /* Write Back UNIT */
        while (!writeBackQ.empty()){
            string curInst = writeBackQ.front();
            executeInst(curInst, -1);
            writeBackQ.pop();
        }

        /* Output simulation.txt here */
        outputSimulation(outputFILE, &cycleCount);
        if (cycleCount == 20)
            break;
        //cout << cycleCount << endl;
    }
    outputFILE.close();
}

void outputSimulation (ofstream& outputFILE, int* cycleCounts){
    map<int, int>::iterator it;
    int i, j;
    int qSize = 0;
    queue<string> tmpQ;
    queue<string> empty;

    //*cycleCounts = *cycleCounts + 1;

    outputFILE << "--------------------" << newline;
    outputFILE << "Cycle " << *cycleCounts << ":" << newline << newline;
    /* IF Unit */
    outputFILE << "IF Unit:" << newline;
    if (IFWaitInst.empty() && IFExecIns.empty()){
        outputFILE << "\tWaiting Instruction:" << newline;
        outputFILE << "\tExecuted Instruction:" << newline;
    } else if (!IFWaitInst.empty() && IFExecIns.empty()){
        outputFILE << "\tWaiting Instruction: [" << disessamblyInst(IFWaitInst) << "]" << newline;
        outputFILE << "\tExecuted Instruction:" << newline;
    } else if (!IFExecIns.empty() && IFWaitInst == "lastwaitcycle"){
        outputFILE << "\tWaiting Instruction:" << newline;
        outputFILE << "\tExecuted Instruction: [" << disessamblyInst(IFExecIns) << "]" << newline;
        IFWaitInst = "";
    } else if (!IFExecIns.empty() && IFWaitInst.empty()){
        outputFILE << "\tWaiting Instruction:" << newline;
        outputFILE << "\tExecuted Instruction: [" << disessamblyInst(IFExecIns) << "]" << newline;
    }
    /* Pre-Issue Queue */
    outputFILE << "Pre-Issue Queue:" << newline;
    swap(tmpQ, empty);
    tmpQ = preIssueQ;
    qSize = tmpQ.size();
    for (i = 0 ; i < qSize ; i++){
        outputFILE << "\tEntry " << i << ": [" << disessamblyInst(tmpQ.front()) << "]" << newline;
        tmpQ.pop();
    }
    for (j = i ; j < PREISSUEQ_SIZE ; j++){
        outputFILE << "\tEntry " << j << ":" << newline;
    }

    /* Pre-ALU1 Queue */
    outputFILE << "Pre-ALU1 Queue:" << newline;
    swap(tmpQ, empty);
    tmpQ = preALU1Q;
    qSize = tmpQ.size();
    for (i = 0 ; i < qSize ; i++){
        outputFILE << "\tEntry " << i << ": [" << disessamblyInst(tmpQ.front()) << "]" << newline;
        tmpQ.pop();
    }
    for (j = i ; j < PREALU1Q_SIZE ; j++){
        outputFILE << "\tEntry " << j << ":" << newline;
    }
    /* pre MEM queue */
    if (!preMEMQ.empty())
        outputFILE << "Pre-MEM Queue: [" << disessamblyInst(preMEMQ) << "]" << newline;
    else
        outputFILE << "Pre-MEM Queue:" << newline;

    /* pre MEM queue */
    if (!postMEMQ.empty())
        outputFILE << "Post-MEM Queue: [" << disessamblyInst(postMEMQ) << "]" << newline;
    else
        outputFILE << "Post-MEM Queue:" << newline;

    /* Pre-ALU2 Queue */
    outputFILE << "Pre-ALU2 Queue:" << newline;
    swap(tmpQ, empty);
    tmpQ = preALU2Q;
    qSize = tmpQ.size();
    for (i = 0 ; i < qSize ; i++){
        outputFILE << "\tEntry " << i << ": [" << disessamblyInst(tmpQ.front()) << "]" << newline;
        tmpQ.pop();
    }
    for (j = i ; j < PREALU2Q_SIZE ; j++){
        outputFILE << "\tEntry " << j << ":" << newline;
    }
    
    /* Post-ALU2 Queue */
    if (!postALU2Q.empty())
        outputFILE << "Post-ALU2 Queue: [" << disessamblyInst(postALU2Q) << "]" << newline;
    else
        outputFILE << "Post-ALU2 Queue:" << newline;

    /* Registers Map*/
    outputFILE << newline << "Registers";
    for (int i = 0 ; i < 32 ; i++){
        if (i % 8 == 0){
                outputFILE << newline;
                outputFILE << "R" << setfill('0') << setw(2) << i << ":";
        }
        outputFILE << "\t" << regMap[i];
    }
    outputFILE << newline;

    /* Data Map */
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
    outputFILE << newline;

}


int getInstCode (string inst){
    string instCat = inst.substr(0,2);
    string instOp = inst.substr(2,4);
    int instCategory = (instCat.compare("01") == 0) ? eInstCatOne : eInstCatTwo;
    return instHash(instOp, instCategory);
}

bool isBranchInst (string inst){
    int instCode = getInstCode(inst);
    if (instCode == eInstJ || instCode == eInstJR || instCode == eInstBEQ || instCode == eInstBGTZ || instCode == eInstBLTZ)
        return true;         
    else
        return false;
}

bool isLWSWInst (string inst){
    int instCode = getInstCode(inst);
    if (instCode == eInstLW || instCode == eInstSW)
        return true;         
    else
        return false;
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
                    sbRegReading.erase(rsI);
                    sbRegReading.erase(rtI);
                    break;
                case eInstLW:
                    regMap[rtI] = dataMap[regMap[rsI] + addrOffsetInt(inst, 16, 0)];
                    sbRegReading.erase(rsI);
                    sbRegWriting.erase(rtI);
                    break;
                case eInstSLL:
                    regMap[regOffsetInt(inst, 16, 5)] = regMap[rtI] << regOffsetInt(inst, 21, 5);
                    sbRegReading.erase(rtI);
                    sbRegWriting.erase(rdI);
                    if (sbRegWriting.count(16))
                        cout << "16 exist in set" << endl;
                    cout << "[executeINst] SSLrtI: " << rtI << " rdI: " << rdI << endl;
                    break;
                case eInstSRL:
                    tmp = (unsigned int) regMap[rtI] >> regOffsetInt(inst, 21, 5);
                    regMap[regOffsetInt(inst, 16, 5)] = (int) tmp;
                    sbRegReading.erase(rtI);
                    sbRegWriting.erase(rdI);
                    break;
                case eInstSRA:
                    regMap[regOffsetInt(inst, 16, 5)] = regMap[rtI] >> regOffsetInt(inst, 21, 5);
                    sbRegReading.erase(rtI);
                    sbRegWriting.erase(rdI);
                    break;
                case eInstNOP: /* SLL R0 R0 0*/
                    break;                                                           
            }
        break;
        
        case eInstCatTwo:
            switch (instHash(instOp, eInstCatTwo)){
                case eInstADD:
                    regMap[rdI] = regMap[rsI] + regMap[rtI];
                    sbRegReading.erase(rsI);
                    sbRegReading.erase(rtI);
                    sbRegWriting.erase(rdI);
                    break;
                case eInstSUB:
                    regMap[rdI] = regMap[rsI] - regMap[rtI];
                    sbRegReading.erase(rsI);
                    sbRegReading.erase(rtI);
                    sbRegWriting.erase(rdI);
                    break;
                case eInstMUL:
                    regMap[rdI] = regMap[rsI] * regMap[rtI];
                    sbRegReading.erase(rsI);
                    sbRegReading.erase(rtI);
                    sbRegWriting.erase(rdI);
                    break;
                case eInstAND:
                    regMap[rdI] = regMap[rsI] & regMap[rtI];
                    sbRegReading.erase(rsI);
                    sbRegReading.erase(rtI);
                    sbRegWriting.erase(rdI);
                    break;
                case eInstOR:
                    regMap[rdI] = regMap[rsI] | regMap[rtI];
                    sbRegReading.erase(rsI);
                    sbRegReading.erase(rtI);
                    sbRegWriting.erase(rdI);
                    break;
                case eInstXOR:
                    regMap[rdI] = regMap[rsI] ^ regMap[rtI];
                    sbRegReading.erase(rsI);
                    sbRegReading.erase(rtI);
                    sbRegWriting.erase(rdI);
                    break;
                case eInstNOR:
                    regMap[rdI] = ~(regMap[rsI] | regMap[rtI]);
                    sbRegReading.erase(rsI);
                    sbRegReading.erase(rtI);
                    sbRegWriting.erase(rdI);
                    break;
                case eInstSLT:
                    if (regMap[rsI] < regMap[rtI])
                        regMap[rdI] = 1;
                    else
                        regMap[rdI] = 0;
                    sbRegReading.erase(rsI);
                    sbRegReading.erase(rtI);
                    sbRegWriting.erase(rdI);
                    break;
                case eInstADDI:
                    regMap[rtI] = regMap[rsI] + convert2sComplement(inst.substr(16), 16);
                    sbRegReading.erase(rsI);
                    sbRegWriting.erase(rtI);
                    break;
                case eInstANDI:
                    regMap[rtI] = regMap[rsI] & addrOffsetInt(inst, 16, 0);
                    sbRegReading.erase(rsI);
                    sbRegWriting.erase(rtI);
                    break;
                case eInstORI:
                    regMap[rtI] = regMap[rsI] | addrOffsetInt(inst, 16, 0);
                    sbRegReading.erase(rsI);
                    sbRegWriting.erase(rtI);
                    break;
                case eInstXORI:
                    regMap[rtI] = regMap[rsI] ^ addrOffsetInt(inst, 16, 0);
                    sbRegReading.erase(rsI);
                    sbRegWriting.erase(rtI);
                    break;
            }
            
        break;
    }
    if (ret == PC)
        return ret + 4;
    else
        return ret;
}

// issue the instrution and check hazards with already issued instructions and do scoreboarding
bool tryIssueInst (string inst){
    //bool ret = false;
    unsigned int tmp;
    string instCat = inst.substr(0,2);
    string instOp = inst.substr(2,4);
    int rsI = regOffsetInt(inst, 6, 5);
    int rtI = regOffsetInt(inst, 11, 5);  //destination for SW LW
    int rdI = regOffsetInt(inst, 16, 5);


    int category = (instCat.compare("01") == 0) ? eInstCatOne : eInstCatTwo;
    switch (category){
        case eInstCatOne: /* Category-1 */
            switch (instHash(instOp, eInstCatOne)){
                case eInstSW:
                    // check RAW/WAW hazard for SW
                    if (isRAW(rsI) || isRAW(rtI))
                        return false;
                    sbRegReading.insert(rsI);
                    sbRegReading.insert(rtI);
                    break;
                case eInstLW:
                    //cout << "[tryIssueInst] LW rsI: " << rsI << " rtI: " << rtI << endl;
                    
                    if (isRAW(rsI) || isWAW(rtI) || isWAR(rtI))
                        return false;
                    sbRegReading.insert(rsI);
                    sbRegWriting.insert(rtI);
                    
                    break;
                case eInstSLL:
                case eInstSRL:
                case eInstSRA:
                    if (isRAW(rtI) || isWAW(rdI) || isWAR(rdI))
                        return false;
                    sbRegReading.insert(rtI);
                    sbRegWriting.insert(rdI);
                    //cout << "[tryIssueInst] SSLrtI: " << rtI << " rdI: " << rdI << endl;
                    //regMap[regOffsetInt(inst, 16, 5)] = regMap[rtI] << regOffsetInt(inst, 21, 5);
                    break;                                                    
            }
        break;
        
        case eInstCatTwo:
            switch (instHash(instOp, eInstCatTwo)){
                case eInstADD:
                case eInstSUB:
                case eInstMUL:
                case eInstAND:
                case eInstOR:
                case eInstXOR:
                case eInstNOR:
                case eInstSLT:
                    // RAW for source, WAW, WAR for destination
                    //cout << "[tryIssueInst] ADD rsI: " << rsI << " rtI: " << rtI << " rdI: " << rdI << newline;
                    //cout << "1. " << isRAW(rsI) << " 2. " << isRAW(rtI) << " 3. " << isWAW(rdI) << " 3. " << isWAR(rdI) << endl;
                    if (isRAW(rsI) || isRAW(rtI) || isWAW(rdI) || isWAR(rdI))
                        return false;
                    sbRegReading.insert(rsI);
                    sbRegReading.insert(rtI);
                    sbRegWriting.insert(rdI);
                    //regMap[rdI] = regMap[rsI] + regMap[rtI];
                    break;

                case eInstADDI:
                case eInstANDI:
                case eInstORI:
                case eInstXORI:
                    if (isRAW(rsI) || isWAW(rtI) || isWAR(rtI))
                        return false;
                    sbRegReading.insert(rsI);
                    sbRegWriting.insert(rtI);
                    //regMap[rtI] = regMap[rsI] + convert2sComplement(inst.substr(16), 16);
                    break;
            }
            
        break;
    }

    return true;
}


bool isRAW (int reg){
    if (sbRegWriting.count(reg) == 1)
        return true;
    else
        return false;
}
bool isWAW (int reg){
    if (sbRegWriting.count(reg) == 1)
        return true;
    else
        return false;
}
bool isWAR (int reg){
    // dont check WAR for already issued instructions
    return false;
    if (sbRegReading.count(reg) == 1)
        return true;
    else
        return false;
}

bool isPreIssueRAW (int reg){
    if (preissueRegWrite.count(reg) == 1)
        return true;
    else
        return false;
}
bool isPreIssueWAW (int reg){
    if (preissueRegWrite.count(reg) == 1)
        return true;
    else
        return false;
}
bool isPreIssueWAR (int reg){
    if (preissueRegRead.count(reg) == 1)
        return true;
    else
        return false;
}

bool checkNonIssuedInst (string inst, bool check, bool sameCycleIssueInsCheck){
    //bool ret = false;
    unsigned int tmp;
    string instCat = inst.substr(0,2);
    string instOp = inst.substr(2,4);
    int rsI = regOffsetInt(inst, 6, 5);
    int rtI = regOffsetInt(inst, 11, 5);  //destination for SW LW
    int rdI = regOffsetInt(inst, 16, 5);

    int category = (instCat.compare("01") == 0) ? eInstCatOne : eInstCatTwo;
    switch (category){
        case eInstCatOne: /* Category-1 */
            switch (instHash(instOp, eInstCatOne)){
                case eInstSW:
                    if (check){
                        if (sameCycleIssueInsCheck){
                            return true;
                        } else {
                            if (isPreIssueRAW(rsI) || isPreIssueRAW(rtI))
                                return false;
                        }
                    } else {
                        preissueRegRead.insert(rsI);
                        preissueRegRead.insert(rtI);
                    }
                    break;
                case eInstLW:
                    if (check){
                        if (isPreIssueRAW(rsI) || isPreIssueWAW(rtI) || isPreIssueWAR(rtI))
                            return false;
                    } else {
                        preissueRegRead.insert(rsI);
                        preissueRegWrite.insert(rtI);
                    }
                    break;
                case eInstSLL:
                case eInstSRL:
                case eInstSRA:
                    if (check){
                        if (isPreIssueRAW(rtI) || isPreIssueWAW(rdI) || isPreIssueWAR(rdI))
                            return false;
                    } else {
                        preissueRegRead.insert(rtI);
                        preissueRegWrite.insert(rdI);
                    }
                    break;                                                 
            }
        break;
        
        case eInstCatTwo:
            switch (instHash(instOp, eInstCatTwo)){
                case eInstADD:
                case eInstSUB:
                case eInstMUL:
                case eInstAND:
                case eInstOR:
                case eInstXOR:
                case eInstNOR:
                case eInstSLT:
                    if (check){
                        if (isPreIssueRAW(rsI) || isPreIssueRAW(rtI) || isPreIssueWAW(rdI) || isPreIssueWAR(rdI))
                            return false;
                    } else {
                        preissueRegRead.insert(rsI);
                        preissueRegRead.insert(rtI);
                        preissueRegWrite.insert(rdI);
                    }   
                    break;
                
                case eInstADDI:
                case eInstANDI:
                case eInstORI:
                case eInstXORI:
                    if (check){
                        if (isPreIssueRAW(rsI) || isPreIssueWAW(rtI) || isPreIssueWAR(rtI))
                            return false;
                    } else {
                        preissueRegRead.insert(rsI);
                        preissueRegWrite.insert(rtI);
                    }
                    break;
            }
        break;
    }
    return true;
}

bool checkBranchInst (string inst){
    //unsigned int tmp;
    string instCat = inst.substr(0,2);
    string instOp = inst.substr(2,4);
    int rsI = regOffsetInt(inst, 6, 5);
    int rtI = regOffsetInt(inst, 11, 5);  //destination for SW LW
    int rdI = regOffsetInt(inst, 16, 5);
    int category = (instCat.compare("01") == 0) ? eInstCatOne : eInstCatTwo;
    switch (category){
        case eInstCatOne: /* Category-1 */
            switch (instHash(instOp, eInstCatOne)){
                case eInstJR:
                case eInstBLTZ:
                case eInstBGTZ:
                    if (isRAW(rsI) || isPreIssueRAW(rsI))
                        return false;
                    //ret= regMap[rsI];
                    break;
                case eInstBEQ:
                    if (isRAW(rsI) || isPreIssueRAW(rsI) || isRAW(rtI) || isPreIssueRAW(rtI))
                        return false;
                    //if (regMap[rsI] == regMap[rtI])
                    //    ret = PC + 4 + addrOffsetInt(inst, 16, 2);
                    break;                                       
            }
        break;
        
    }
    return true;
}