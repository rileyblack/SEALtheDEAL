#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <math.h>

#define OVERDRAFT_LIMIT 5000

/*
 * struct to hold each accounts info
 */
typedef struct account{
        int balance;			//variable to hold account balance
        int transaction_count;	//variable to hold number of transactions this account has underwent
        int account_number;		//variable to hold the account number of the current account in question
        int type;				//variable to hold the account type of the current account in question (0 for business, 1 for personal, -1 for error)
        int deposit_fee;		//variable to hold the deposit fee of the current account in question
        int withdraw_fee;		//variable to hold the withdrawl fee of the current account in question
        int transfer_fee;		//variable to hold the transfer fee of the current account in question
        int transaction_cap;	//variable to hold the transaction capacity before charge of the current account in question
        int excess_fee;			//variable to hold the transaction fee incurred once the capacity is reached of the current account in question
        int overdraft_boolean;	//variable to reflect if the current account in question permits overdraft (0 for 'N', 1 for 'Y', -1 for error)
        int overdraft_fee;		//variable to hold the overdraft fee per 500$ of overdraft of the current account in question, if overdraft is permitted
        int overdraft_applied;	//variable holding up to what interval of 500 from -1 to -5000 the overdraft fee has been applied
} Account;

/*
 * Generic deposit/withdraw/transfer
 */
typedef struct operation{
        int id_incoming;	//if operation involves money going into an account, this is id of that account
        int id_outgoing;	//if operation involves money going out of an account, this is id of that account
        int amount;			//amount being moved
        int operation_type; //1 = deposit, 2 = withdraw, 3 = transfer, -1 = undefined
} Operation;

/*
 * Generic depositor/client
 */
typedef struct requestor{
        int type;				//0 = depositor, 1 = client
        int id;					//requestor id
        int operation_count;	//number of operations this requestor preforms
        Operation * operations; //array of operations (deposits) that this requestor preforms
} Requestor;

/*
 * Function to process each transaction from a request (line) from a depositor or a client alike
 */
void * request(void * interactor);

/*
 * Function to funnel transactions to proper handling functions (deposit, withdraw, transfer)
 */
void transaction(Operation op);

/*
 * Function to handle a deposit request
 */
void deposit(int account_id, int amount);

/*
 * Function to handle a withdraw request
 */
void withdraw(int account_id, int amount);

/*
 * Function to handle a transfer request
 */
void transfer(int incoming_account_id, int outgoing_account_id, int amount);

/*
 * Function to compute to overdraft fees applicable to a transaction
 */
int overdraft(int account_id, int beginning_balance, int ending_balance, int fee);

Account * accounts;		//shared accounts between threads

pthread_mutex_t lock;	//mutex lock

int main(){
        /*
         * File extraction portion of the code is below, following this methodology:
         *
         * 1. Open the file, and determine the amount of account lines, depositor lines, and client lines,
         *    and hold integer value in respective variables
         * 2. Dynamically create two arrays, one for depositors and one for clients, of the size of the
         *    number of respective lines for each
         * 3. Reopen the file, and count the number of operations that each depositor/client line has,
         *    and populate the array with that integer value at the index representing that depositor/
         *    client line (ie. client_array[0] hold number of operations in first client line in input file)
         * 4. Dynamically create three arrays, one for accounts, one for depositors and one for clients, of
         *    the size of the number of respective lines for each
         * 5. Reopen the file, and parse through the file for the critical values in each type of line, and
         *    create an account/depositor/client struct with the attrbutes specified in the input file, and
         *    place struct in respective array at proper index (ie. accounts[1] has the first account info
         *    in the input file
         * 6. Once concluded, should have three arrays accounts[], depositors[], and clients[] that are the
         *    dynamic size of the amount of account/depositors/clients specified in the input file, with
         *    each element in each array containing the information needed
         */

        FILE * file_ptr = NULL;
        char character; //generic character used to read in characters from the file that do not need to be saved
        char string[1024]; //generic string used to read in strings from the file that do not need to be saved

        file_ptr = fopen("assignment_3_input_file.txt", "r");
        if (file_ptr == NULL) { //check if file was opened without error
                printf("\n\nError opening file.\n\n"); //if an error occured, print error message and end program
                exit(0);
        }

        int total_accounts = 0; //variable to hold the number of accounts specified by the input file

        character = fgetc(file_ptr); //retrieve first character

        while((character != EOF)&&(character != 'a')){ //find first 'a' character indicating account info is starting
                character = fgetc(file_ptr); //while proper account indicator is not found, get next character from input file
        }
        if(character == EOF){ //if the end of the file is reached without an 'a' indicator being found, input file format is wrong
                printf("\n\nInput file is not in correct format.\n\n");
                return 0;
        }

        total_accounts = total_accounts + 1; //first 'a' is found, indicating at least 1 account

        character = fgetc(file_ptr); //retrieve next character

        while(character != EOF){ //iterate through each remaining character in file until the end of file character
                if(character == '\n'){ //check if a newline character has been found, indicating an oppurtunity for potential 'a' next character
                        character = fgetc(file_ptr); //retrieve next character
                        if(character == 'a'){ //check if character starting new line is an 'a', indicating a new account
                                total_accounts = total_accounts + 1; //if 'a' is found, 1 more account
                        }
                        else{
                                break; //if an 'a' is not found at the start of a new line, the account lines of the input file are complete
                        }
                }
                character = fgetc(file_ptr); //if not a newline character, keep searching for one
        }
        if(character == EOF){ //if the end of the file is reached by above loop, input file format is wrong
                printf("\n\nInput file is not in correct format.\n\n");
                return 0;
        }

        int total_depositors = 0; //variable to hold the number of initial depositers specified by the input file

        while((character != EOF)&&(character != 'd')){ //find first 'd' character (from dep)  after account lines indicating deposit info is starting
                character = fgetc(file_ptr); //while proper deposit indicator is not found, get next character from input file
        }
        if(character == EOF){ //if the end of the file is reached without a 'd' indicator (from dep) being found, input file format is wrong
                printf("\n\nInput file is not in correct format.\n\n");
                return 0;
        }

        total_depositors = total_depositors + 1; //first 'd' (from dep) is found, indicating at least 1 deposit

        character = fgetc(file_ptr); //retrieve next character

        while(character != EOF){ //iterate through each remaining character in file until the end of file character
                if(character == '\n'){ //check if a newline character has been found, indicating an oppurtunity for potential 'd' (from dep)
                        character = fgetc(file_ptr); //retrieve next character
                        if(character == 'd'){ //check if character starting new line is an 'd', indicating a new deposit
                                total_depositors = total_depositors + 1; //if 'd' is found, 1 more deposit
                        }
                        else{
                                break; //if a 'd' is not found at the start of a new line, the deposit lines of the input file are complete
                        }
                }
                character = fgetc(file_ptr); //if not a newline character, keep searching for one
        }
        if(character == EOF){ //if the end of the file is reached by above loop, input file format is wrong
                printf("\n\nInput file is not in correct format.\n\n");
                return 0;
        }

        int total_clients = 0; //variable to hold the number of clients specified by the input file

        while((character != EOF)&&(character != 'c')){ //find first 'c' character indicating client info is starting
                character = fgetc(file_ptr); //while proper client indicator is not found, get next character from input file
        }
        if(character == EOF){ //if the end of the file is reached without a 'c' indicator being found, input file format is wrong
                printf("\n\nInput file is not in correct format.\n\n");
                return 0;
        }

        total_clients = total_clients + 1; //first 'c' is found, indicating at least 1 client

        character = fgetc(file_ptr); //retrieve next character

        while(character != EOF){ //iterate through each remaining character in file until the end of file character
                if(character == '\n'){ //check if a newline character has been found, indicating an oppurtunity for potential 'c'
                        character = fgetc(file_ptr); //retrieve next character
                        if(character == 'c'){ //check if character starting new line is an 'c', indicating a new client
                                total_clients = total_clients + 1; //if 'c' is found, 1 more client
                        }
                        else{
                                break; //if a 'c' is not found at the start of a new line, the client lines of the input file are complete
                        }
                }
                character = fgetc(file_ptr); //if not a newline character, keep searching for one
        }

        fclose(file_ptr); //close file

        int * depositor_operation_count = malloc(sizeof(int) * total_depositors); //array of integers that will hold that number of operations that
                                                                                  //each depositer has, for example depositer_operation_count[0] = 2 means
                                                                                  //that depositer 1 has 2 operations

        int * client_operation_count = malloc(sizeof(int) * total_clients); //array of integers that will hold that number of operations that
                                                                            //each client has, for example client_operation_count[0] = 2 means
                                                                            //that client 1 has 2 operations

        int operation_counter = 0;	//counter used to count operations for the client in question from the input file
        int depositor_id = 0;		//counter used to keep track of which depositer the operations are currently being counted for
        int client_id = 0;			//counter used to keep track of which client the operations are currently being counted for

        file_ptr = fopen("assignment_3_input_file.txt", "r");
        if (file_ptr == NULL) { //check if file was opened without error
                printf("\n\nError opening file.\n\n"); //if an error occured, print error message and end program
                exit(0);
        }

        character = fgetc(file_ptr); //get first character

        while(character != EOF){ //find first 'd' character indicating depositer info is starting
                if(character == '\n'){ //check if a newline character has been found, indicating an oppurtunity for potential 'd'
                        character = fgetc(file_ptr); //retrieve next character
                        if(character == 'd'){ //check if character starting new line is an 'd', indicating a new depositer
                                break; //if character is 'd', we have found start of depositer lines so break
                        }
                }
                character = fgetc(file_ptr); //if not a newline character, keep searching
        }

        while(character != EOF){ //loop for each depositer
                operation_counter = 0; //reset operation counter for new depositer
                character = fgetc(file_ptr); //retrieve next character
                while(character != EOF){  //loop for each operation for current depositer
                        if(character == '\n'){ //check if a newline character has been found, indicating an oppurtunity for potential 'd'
                                character = fgetc(file_ptr); //retrieve next character
                                if(character == 'd'){ //check if character starting new line is an 'd', indicating a new depositer
                                        break; //if a new depositer, operation count complete
                                }
                                else if(character == 'c'){ //check if character starting new line is an 'c', indicating depositer info is done
                                        break;
                                }
                        }
                        else if(character == 'd'){ //if operation indicator, increment counter
                                operation_counter = operation_counter + 1;
                        }
                        character = fgetc(file_ptr); //get next character
                }
                depositor_operation_count[depositor_id] = operation_counter; //fill corresponding depositor index with count found
                depositor_id = depositor_id + 1; //go to next depositor index
                if(character == 'c'){ //if we have reach the end of depositor info, break
                        break;
                }
        }

        while(character != EOF){ //loop for each client
                operation_counter = 0; //reset operation counter for new client
                character = fgetc(file_ptr); //retrieve next character
                while(character != EOF){  //loop for each operation for current client
                        if(character == '\n'){ //check if a newline character has been found, indicating an oppurtunity for potential 'c'
                                character = fgetc(file_ptr); //retrieve next character
                                if(character == 'c'){ //check if character starting new line is an 'c', indicating a new client
                                        break; //if a new client, operation count complete
                                }
                        }
                        else if((character == 'd')||(character == 't')||(character == 'w')){ //if operation indicator, increment counter
                                operation_counter = operation_counter + 1;
                        }
                        character = fgetc(file_ptr); //get next character
                }
                client_operation_count[client_id] = operation_counter; //fill corresponding client index with count found
                client_id = client_id + 1; //go to next clients index
        }

        fclose(file_ptr); //close file

        accounts = malloc(sizeof(Account) * total_accounts);					//array of accounts
        Requestor * depositors = malloc(sizeof(Requestor) * total_depositors);	//array of depositors
        Requestor * clients = malloc(sizeof(Requestor) * total_clients);		//array of clients

        int account_id = 0; //variable to keep track of account number

        file_ptr = fopen("assignment_3_input_file.txt", "r");
        if (file_ptr == NULL) { //check if file was opened without error
                printf("\n\nError opening file.\n\n"); //if an error occured, print error message and end program
                exit(0);
        }

        character = fgetc(file_ptr); //retrieve first character

        int account_number;		//variable to hold the account number of the current account in question
        char type[1024];		//variable to hold the account type of the current account in question
        int deposit_fee;		//variable to hold the deposit fee of the current account in question
        int withdraw_fee;		//variable to hold the withdrawl fee of the current account in question
        int transfer_fee;		//variable to hold the transfer fee of the current account in question
        int transaction_cap;	//variable to hold the transaction capacity before charge of the current account in question
        int excess_fee;			//variable to hold the transaction fee incurred once the capacity is reached of the current account in question
        char overdraft_boolean; //variable to reflect if the current account in question permits overdraft (0 for 'N', 1 for 'Y')
        int overdraft_fee;		//variable to hold the overdraft fee per 500$ of overdraft of the current account in question, if overdraft is permitted

        while((character != EOF)&&(character != 'd')){ //loop through all lines until they begin with d

                while((character != EOF)&&(character != 'a')){ //find first 'a' character indicating account info is starting
                        character = fgetc(file_ptr); //while proper account indicator is not found, get next character from input file
                }
                if(character == EOF){ //if the end of the file is reached without an 'a' indicator being found, input file format is wrong
                        printf("\n\nInput file is not in correct format1.\n\n");
                        return 0;
                }

                fscanf(file_ptr, "%d", &account_number); //retrieve account number and store in variable
                if((account_number < 1)||(account_number > INT_MAX)){ //check if retrieved value is a number as expected
                        printf("\n\nInput file is not in correct format2.\n\n"); //if not a number, then input file format is wrong
                        return 0;
                }

                fscanf(file_ptr, " %1023s", string); //retrieve next word from input file
                if(strcmp(string, "type")){ //check if next word is what would be expected from properly formatted input file
                        printf("\n\nInput file is not in correct format.\n\n"); //if the next word is not 'type' then input file format is wrong
                        return 0;
                }

                fscanf(file_ptr, " %1023s", type); //retrieve next word from input file
                if((strcmp(type, "business"))&&(strcmp(type, "personal"))){ //check if next word is what would be expected from properly formatted input file
                        printf("\n\nInput file is not in correct format.\n\n"); //if the next word is not 'business' or 'personal' then input file format is wrong
                        return 0;
                }

                character = fgetc(file_ptr); //retrieve next character

                while((character != EOF)&&(character != 'd')){ //find 'd' character indicating account deposit fee
                        character = fgetc(file_ptr); //while proper account indicator is not found, get next character from input file
                }
                if(character == EOF){ //if the end of the file is reached without a 'd' indicator being found, input file format is wrong
                        printf("\n\nInput file is not in correct format.\n\n");
                        return 0;
                }

                fscanf(file_ptr, "%d", &deposit_fee); //retrieve deposit fee and store in variable
                if((deposit_fee < 0)||(deposit_fee > INT_MAX)){ //check if retrieved value is a number as expected
                        printf("\n\nInput file is not in correct format.\n\n"); //if not a number, then input file format is wrong
                        return 0;
                }

                character = fgetc(file_ptr); //retrieve next character

                while((character != EOF)&&(character != 'w')){ //find 'w' character indicating account withdraw fee
                        character = fgetc(file_ptr); //while proper account indicator is not found, get next character from input file
                }
                if(character == EOF){ //if the end of the file is reached without a 'w' indicator being found, input file format is wrong
                        printf("\n\nInput file is not in correct format.\n\n");
                        return 0;
                }

                fscanf(file_ptr, "%d", &withdraw_fee); //retrieve withdraw fee and store in variable
                if((withdraw_fee < 0)||(withdraw_fee > INT_MAX)){ //check if retrieved value is a number as expected
                        printf("\n\nInput file is not in correct format.\n\n"); //if not a number, then input file format is wrong
                        return 0;
                }

                character = fgetc(file_ptr); //retrieve next character

                while((character != EOF)&&(character != 't')){ //find 't' character indicating account transfer fee
                        character = fgetc(file_ptr); //while proper account indicator is not found, get next character from input file
                }
                if(character == EOF){ //if the end of the file is reached without a 't' indicator being found, input file format is wrong
                        printf("\n\nInput file is not in correct format.\n\n");
                        return 0;
                }

                fscanf(file_ptr, "%d", &transfer_fee); //retrieve transfer fee and store in variable
                if((transfer_fee < 0)||(transfer_fee > INT_MAX)){ //check if retrieved value is a number as expected
                        printf("\n\nInput file is not in correct format.\n\n"); //if not a number, then input file format is wrong
                        return 0;
                }

                fscanf(file_ptr, " %1023s", string); //retrieve next word from input file
                if(strcmp(string, "transactions")){ //check if next word is what would be expected from properly formatted input file
                        printf("\n\nInput file is not in correct format.\n\n"); //if the next word is not 'transactions' then input file format is wrong
                        return 0;
                }

                fscanf(file_ptr, "%d", &transaction_cap); //retrieve transaction cap and store in variable
                if((transaction_cap < 0)||(transaction_cap > INT_MAX)){ //check if retrieved value is a number as expected
                        printf("\n\nInput file is not in correct format.\n\n"); //if not a number, then input file format is wrong
                        return 0;
                }

                fscanf(file_ptr, "%d", &excess_fee); //retrieve excess transaction fee and store in variable
                if((excess_fee < 0)||(excess_fee > INT_MAX)){ //check if retrieved value is a number as expected
                        printf("\n\nInput file is not in correct format.\n\n"); //if not a number, then input file format is wrong
                        return 0;
                }

                fscanf(file_ptr, " %1023s", string); //retrieve next word from input file
                if(strcmp(string, "overdraft")){ //check if next word is what would be expected from properly formatted input file
                        printf("\n\nInput file is not in correct format.\n\n"); //if the next word is not 'overdraft' then input file format is wrong
                        return 0;
                }

                overdraft_boolean = fgetc(file_ptr); //retrieve next character

                while((overdraft_boolean != EOF)&&(overdraft_boolean != 'Y')&&(overdraft_boolean != 'N')){  //find 'Y' or 'N' indicating account overdraft allowance
                        overdraft_boolean = fgetc(file_ptr); //while proper account indicator is not found, get next character from input file
                }
                if(overdraft_boolean == EOF){ //if the end of the file is reached without a 'Y' or 'N' indicator being found, input file format is wrong
                        printf("\n\nInput file is not in correct format.\n\n");
                        return 0;
                }

                if(overdraft_boolean == 'Y'){ //if overdraft is allowed, must get the overdraft fee
                        fscanf(file_ptr, "%d", &overdraft_fee); //retrieve account number and store in variable
                        if((overdraft_fee < 0)||(overdraft_fee > INT_MAX)){ //check if retrieved value is a number as expected
                                printf("\n\nInput file is not in correct format.\n\n"); //if not a number, then input file format is wrong
                                return 0;
                        }
                }
                else{ //if overdraft is not allowed, set fee to 0
                        overdraft_fee = 0;
                }

                character = fgetc(file_ptr); //retrieve next character
                while((character != EOF)&&(character != '\n')){ //continue until the end of the line
                        character = fgetc(file_ptr); //while proper account indicator is not found, get next character from input file
                }
                if(character == EOF){ //if the end of the file is reached input file format is wrong
                        printf("\n\nInput file is not in correct format.\n\n");
                        return 0;
                }

                int account_type_int; //variable to reflect the account type as an integer (0 business, 1 personal)

                if(!(strcmp(type, "business"))){ //if business, 1
                        account_type_int = 0;
                }
                else if(!(strcmp(type, "personal"))){ //if personal, 2
                        account_type_int = 1;
                }
                else{
                        account_type_int = -1;  //if error, set -1 flag
                }

                int overdraft_boolean_int; //variable to reflect the overdraft as an integer (0 no, 1 yes)

                if(overdraft_boolean == 'N'){ //if no, 0
                        overdraft_boolean_int = 0;
                }
                else if(overdraft_boolean == 'Y'){ //if yes, 1
                        overdraft_boolean_int = 1;
                }
                else{ //if error, set -1 flag
                        overdraft_boolean_int = -1;
                }

                //create account struct with proper variables (0 for balance, 0 for initial transaction count, 0 levels of overdraft fee applied)
                Account account = {0, 0, account_number, account_type_int, deposit_fee, withdraw_fee, transfer_fee, transaction_cap, excess_fee, overdraft_boolean_int, overdraft_fee, 0};

                accounts[account_id] = account; //put account in proper place in account array

                character = fgetc(file_ptr); //retrieve next character
                account_id = account_id + 1; //update position for next account in account array
        }
        if(character == EOF){ //if the end of the file is reached, input file format is wrong
                printf("\n\nInput file is not in correct format1.\n\n");
                return 0;
        }

        /*
        int j;
        for(j=0;j<total_accounts;j++){
                printf("\na: %d balance: %d type: %d d: %d w: %d t: %d, trans: %d excess: %d overd: %d overfee: %d", accounts[j].account_number, accounts[j].balance, accounts[j].type, accounts[j].deposit_fee, accounts[j].withdraw_fee, accounts[j].transfer_fee, accounts[j].transaction_cap, accounts[j].excess_fee, accounts[j].overdraft_boolean, accounts[j].overdraft_fee);
        }
        */


        depositor_id = 0; //reset depositor id

        int depositor_number; //variable to hold the depositor number of the current depositor in question

        while((character != EOF)&&(character != 'c')){  //loop through all lines until they begin with c

                Operation * temp_operations = malloc(sizeof(Operation) * depositor_operation_count[depositor_id]); //allocate array for operations of this depositor

                character = fgetc(file_ptr); //moving pst e in dep in input file
                character = fgetc(file_ptr); //moving pst p in dep in input file

                fscanf(file_ptr, "%d", &depositor_number); //retrieve depositor number and store in variable
                if((depositor_number < 1)||(depositor_number > INT_MAX)){ //check if retrieved value is a number as expected
                        printf("\n\nInput file is not in correct format2.\n\n"); //if not a number, then input file format is wrong
                        return 0;
                }

                int operation_id = 0;		//variable to keep track of operation number of depositor
                int deposit_account_number; //variable to hold the id of the account the deposit is going into
                int deposit_amount;			//variable to hold the deposit amount of the current deposit in questiond

                character = fgetc(file_ptr); //retrieve next character

                while((character != EOF)&&(character != '\n')){ //loop until line is complete (all operations for depositor complete)

                        while((character != EOF)&&(character != 'a')){ //find 'a' character indicating account number to deposit into is next
                                character = fgetc(file_ptr); //while proper indicator is not found, get next character from input file
                        }
                        if(character == EOF){ //if the end of the file is reached, input file format is wrong
                                printf("\n\nInput file is not in correct format.\n\n");
                                return 0;
                        }

                        fscanf(file_ptr, "%d", &deposit_account_number); //retrieve account deposit number and store in variable
                        if((deposit_account_number < 0)||(deposit_account_number > INT_MAX)){ //check if retrieved value is a number as expected
                                printf("\n\nInput file is not in correct format.\n\n"); //if not a number, then input file format is wrong
                                return 0;
                        }

                        fscanf(file_ptr, "%d", &deposit_amount); //retrieve deposit amount and store in variable
                        if((deposit_amount < 0)||(deposit_amount > INT_MAX)){ //check if retrieved value is a number as expected
                                printf("\n\nInput file is not in correct format.\n\n"); //if not a number, then input file format is wrong
                                return 0;
                        }

                        character = fgetc(file_ptr); //retrieve next character

                        while((character != EOF)&&(character != '\n')&&(character != 'd')){ //find next operation, end of line, or end of file
                                character = fgetc(file_ptr); //while proper account indicator is not found, get next character from input file
                        }

                        //creating operation struct where -1 is indicator that no outgoing account in operation, 1 is deposit indicator
                        Operation operation = {deposit_account_number, -1,  deposit_amount, 1}; //1 = deposit, 2 = withdraw, 3 = transfer, 0 = undefined

                        temp_operations[operation_id] = operation; //put operation in proper place in operation array

                        operation_id = operation_id + 1; //update position for next operation in operation array
                }
                if(character == EOF){ //if the end of the file is reached, input file format is wrong
                        printf("\n\nInput file is not in correct format.\n\n");
                        return 0;
                }
                character = fgetc(file_ptr); //retrieve next character
                while((character != EOF)&&(character != 'd')&&(character != 'c')){ //continue until beginning indicator in next line
                        character = fgetc(file_ptr); //while proper account indicator is not found, get next character from input file
                }

                //0 for depositor
                Requestor depositor = {0, depositor_number, depositor_operation_count[depositor_id], temp_operations}; //create depositor with id count and operations

                depositors[depositor_id] = depositor; //put depositor in proper place in depositor array

                depositor_id = depositor_id + 1; //update position for next deposit in deposit array
        }
        if(character == EOF){ //if the end of the file is reached, input file format is wrong
                        printf("\n\nInput file is not in correct format.\n\n");
                        return 0;
        }

        /*
        int i;
        for(i=0;i<total_depositors;i++){
                for(j=0;j<depositors[i].operation_count;j++){
                printf("\ndep(%d): %d a: %d type: %d amount: %d", depositors[i].type, depositors[i].id, depositors[i].operations[j].id_incoming, depositors[i].operations[j].operation_type,  depositors[i].operations[j].amount);
                }
        }
        */

        client_id = 0; //reset client counter

        int client_number; //variable to hold the depositor number of the current depositor in question

        while(character != EOF){ //until file is depeleted

                Operation * temp_operations = malloc(sizeof(Operation) * client_operation_count[client_id]); //allocate array for operations of this client

                fscanf(file_ptr, "%d", &client_number); //retrieve client number and store in variable

                if((client_number < 1)||(client_number > INT_MAX)){ //check if retrieved value is a number as expected
                        printf("\n\nInput file is not in correct format.\n\n"); //if not a number, then input file format is wrong
                        return 0;
                }

                int operation_id = 0;			//variable to keep track of operation number
                int incoming_account_number;	//variable to hold the id of the account the money is going into
                int outgoing_account_number;	//variable to hold the id of the account the money is coming from
                int amount;						//variable to hold the amount of the current operation in questiond
                int type;						//variable to hold the type of the current opertion in question


                character = fgetc(file_ptr); //retrieve next character

                while((character != EOF)&&(character != '\n')){ //loop until line is complete (all operations for client complete)

                        while((character != EOF)&&(character != 'd')&&(character != 'w')&&(character != 't')){ //find character indicating next operation
                                character = fgetc(file_ptr); //while proper indicator is not found, get next character from input file
                        }
                        if(character == EOF){ //if the end of the file is reached, input file format is wrong
                                printf("\n\nInput file is not in correct format.\n\n");
                                return 0;
                        }

                        if(character == 'd'){		//if deposit, 1
                                type = 1;
                        }
                        else if(character == 'w'){	//if withdraw, 2
                                type = 2;
                        }
                        else if(character == 't'){	//if transfer, 3
                                type = 3;
                        }
                        else{						//if error, 0
                                type = 0;
                        }

                        character = fgetc(file_ptr); //retrieve next character

                        while((character != EOF)&&(character != 'a')){ //find character indicating account
                                character = fgetc(file_ptr); //while proper indicator is not found, get next character from input file
                        }
                        if(character == EOF){ //if the end of the file is reached, input file format is wrong
                                printf("\n\nInput file is not in correct format.\n\n");
                                return 0;
                        }
                        if(type == 1){ //if deposit
                                fscanf(file_ptr, "%d", &incoming_account_number); //retrieve incoming account number and store in variable
                                if((incoming_account_number < 0)||(incoming_account_number > INT_MAX)){ //check if retrieved value is a number as expected
                                        printf("\n\nInput file is not in correct format.\n\n"); //if not a number, then input file format is wrong
                                        return 0;
                                }
                                outgoing_account_number = -1; //no outgoing account in deposit so -1 indicator
                        }
                        else if(type == 2){ //if withdraw
                                fscanf(file_ptr, "%d", &outgoing_account_number); //retrieve outgoing account number and store in variable
                                if((outgoing_account_number < 0)||(outgoing_account_number > INT_MAX)){ //check if retrieved value is a number as expected
                                        printf("\n\nInput file is not in correct format.\n\n"); //if not a number, then input file format is wrong
                                        return 0;
                                }
                                incoming_account_number = -1; //no ingoing account in deposit so -1 indicator
                        }
                        else if(type == 3){ //if transfer
                                fscanf(file_ptr, "%d", &outgoing_account_number); //retrieve outgoing account number and store in variable
                                if((outgoing_account_number < 0)||(outgoing_account_number > INT_MAX)){ //check if retrieved value is a number as expected
                                        printf("\n\nInput file is not in correct format.\n\n"); //if not a number, then input file format is wrong
                                        return 0;
                                }
                                character = fgetc(file_ptr); //retrieve next character

                                while((character != EOF)&&(character != 'a')){ //find character indicating account
                                        character = fgetc(file_ptr); //while proper indicator is not found, get next character from input file
                                }
                                if(character == EOF){ //if the end of the file is reached, input file format is wrong
                                        printf("\n\nInput file is not in correct format.\n\n");
                                }
                                fscanf(file_ptr, "%d", &incoming_account_number); //retrieve incoming account number and store in variable
                                if((incoming_account_number < 0)||(incoming_account_number > INT_MAX)){ //check if retrieved value is a number as expected
                                        printf("\n\nInput file is not in correct format.\n\n"); //if not a number, then input file format is wrong
                                        return 0;
                                }
                        }

                        fscanf(file_ptr, "%d", &amount); //retrieve operation amount and store in variable
                        if((amount < 0)||(amount > INT_MAX)){ //check if retrieved value is a number as expected
                                printf("\n\nInput file is not in correct format.\n\n"); //if not a number, then input file format is wrong
                                return 0;
                        }

                        character = fgetc(file_ptr); //retrieve next character
                        while((character == EOF)&&(character == '\n')&&(character != 'd')&&(character != 'w')&&(character != 't')){ //find next operation indicator
                                character = fgetc(file_ptr); //while proper account indicator is not found, get next character from input file
                        }

                        //creating operation struct with appropriate variables
                        Operation operation = {incoming_account_number, outgoing_account_number, amount, type}; //1 = deposit, 2 = withdraw, 3 = transfer, 0 = undefine

                        temp_operations[operation_id] = operation; //put operation in proper place in operation array

                        operation_id = operation_id + 1; //update position for next operation in operation array
                }
                if(character == EOF){ //if the end of the file is reached, input file format is wrong
                        break;
                }
                character = fgetc(file_ptr); //retrieve next character
                while((character != EOF)&&(character != 'c')){
                        character = fgetc(file_ptr); //while proper account indicator is not found, get next character from input file
                }

                //1 for client
                Requestor client = {1, client_number, client_operation_count[client_id], temp_operations}; //create client struct with id and operation array

                clients[client_id] = client; //put client in proper place in client array

                client_id = client_id + 1; //update position for next client in client array
        }
        /*
        for(i=0;i<total_clients;i++){
                for(j=0;j<clients[i].operation_count;j++){
                printf("\nc(%d): %d type: %d ain: %d aout: %d amount: %d", clients[i].type, clients[i].id, clients[i].operations[j].operation_type, clients[i].operations[j].id_incoming, clients[i].operations[j].id_outgoing, clients[i].operations[j].amount);
                }
        }
        */

        /*
         * This part of the code uses the populated data structures depositors[] and clients[] to create threads to process requests, then terminates the
         * threads and deallocates the dynamic memory
         */
        int err_thread;

        pthread_t depositor_threads[total_depositors]; //thread for each depositor

        if (pthread_mutex_init(&lock, NULL) != 0) {
                printf("\n mutex init failed\n");
                return 1;
        }
        int i;
        for(i=0;i<total_depositors;i++){ //for each depositor
                err_thread = pthread_create(&depositor_threads[i], NULL, &request, &depositors[i]); //create thread
                if (err_thread != 0) { //check if success
                        printf("\n Error creating thread %d", i);
                }
        }

        for (i=0;i<total_depositors;i++) { //for each depositor
                pthread_join(depositor_threads[i], NULL); //join before moving to clients
        }

        pthread_t client_threads[total_clients]; //thread for each client

        for(i=0;i<total_clients;i++){ //for each client
                err_thread = pthread_create(&client_threads[i], NULL, &request, &clients[i]); //create thread
                if (err_thread != 0) { //check if success
                        printf("\n Error creating thread %d", i);
                }
        }

        for (i=0;i<total_clients;i++) { //for each client
                pthread_join(client_threads[i], NULL); //join before deallocating
        }

        pthread_mutex_destroy(&lock);

        file_ptr = fopen("assignment_3_output_file.txt", "w"); // write only

        // test for files not existing.
        if (file_ptr == NULL) {
              printf("\n\nError opening file.\n\n");
              exit(0);
        }

        for(i=0;i<total_accounts;i++){ //for each account
                char * string;
                if(accounts[i].type = 0){ //check if business or personal
                        string = "business";
                }
                else{
                        string = "personal";
                }
                printf("\na%d type %s %d", accounts[i].account_number, string, accounts[i].balance); //print to command prompt
                fprintf(file_ptr, "a%d type %s %d\n", accounts[i].account_number, string, accounts[i].balance); //print to file
        }
        printf("\n");
        fprintf(file_ptr, "\n");
        fclose(file_ptr); //close file

        //free memory assciated with depositor operation arrays
        for(i=0;i<total_depositors;i++){
                free(depositors[i].operations);
        }

        //free memory assciated with depositor operation arrays
        for(i=0;i<total_clients;i++){
                free(clients[i].operations);
        }

        //free data structures used
        free(depositor_operation_count);
        free(client_operation_count);
        free(accounts);
        free(depositors);
        free(clients);

        return 0;
}

void * request(void * requestor) {
        Requestor * temp = (Requestor *) requestor; //extract requestor from void pointer
        Requestor r = *temp; //extract requestor from pointer
        int i;
        for(i=0;i<r.operation_count;i++){ //for each transaction
                transaction(r.operations[i]);
        }
}

void transaction(Operation op) {
        if(op.amount < 0){ //insure that transaction amount is positive
                printf("\nTransaction amount must be greater than zero.");
                return;
        }
        pthread_mutex_lock(&lock);  // ENTRY REGION
        if(op.operation_type == 1){ //type 1 = deposit
                deposit(op.id_incoming - 1, op.amount);
        }
        else if(op.operation_type == 2){ //type 2 = withdraw
                withdraw(op.id_outgoing - 1, op.amount);
        }
        else if(op.operation_type == 3){ //type 3 = transfer
                transfer(op.id_incoming - 1, op.id_outgoing - 1, op.amount);
        }
        else{
                printf("\nOperation not supported.\n");
        }
        pthread_mutex_unlock(&lock); // EXIT REGION
}

void deposit(int account_id, int amount) {
        int beginning_balance = accounts[account_id].balance;
        accounts[account_id].balance += amount; //deduct amount
        accounts[account_id].balance -= accounts[account_id].deposit_fee; //deduct fee

        if(accounts[account_id].transaction_count >= accounts[account_id].transaction_cap){
                accounts[account_id].balance -= accounts[account_id].excess_fee; //deduct fee if required
        }
        int ending_balance = accounts[account_id].balance;
        //compute overdraft
        int overdrafted_fee = overdraft(accounts[account_id].overdraft_applied, beginning_balance, ending_balance, accounts[account_id].overdraft_fee);
        if(overdrafted_fee == -5001){ //if insufficient funds, reimburse
                accounts[account_id].balance = beginning_balance;
                return;
        }
        accounts[account_id].balance -= overdrafted_fee; //deduct fee
        if((accounts[account_id].balance < 0)&&(accounts[account_id].overdraft_boolean == 0)){ //if insufficient funds, reimburse
                accounts[account_id].balance = beginning_balance;
                return;
        }
        if(accounts[account_id].balance < -5000){ //if insufficient funds, reimburse
                accounts[account_id].balance = beginning_balance;
                return;
        }
        accounts[account_id].transaction_count += 1;
        return;
}

void withdraw(int account_id, int amount) {
        int beginning_balance = accounts[account_id].balance;
        accounts[account_id].balance -= amount; //deduct amount
        accounts[account_id].balance -= accounts[account_id].withdraw_fee; //deduct fee
        if(accounts[account_id].transaction_count >= accounts[account_id].transaction_cap){
                accounts[account_id].balance -= accounts[account_id].excess_fee; //deduct fee if required
        }
        int ending_balance = accounts[account_id].balance;
        //compute overdraft
        int overdrafted_fee = overdraft(accounts[account_id].overdraft_applied, beginning_balance, ending_balance, accounts[account_id].overdraft_fee);
        if(overdrafted_fee == -5001){ //if insufficient funds, reimburse
                accounts[account_id].balance = beginning_balance;
                return;
        }
        accounts[account_id].balance -= overdrafted_fee; //deduct fee
        if((accounts[account_id].balance < 0)&&(accounts[account_id].overdraft_boolean == 0)){ //if insufficient funds, reimburse
                accounts[account_id].balance = beginning_balance;
                return;
        }
        if(accounts[account_id].balance < -5000){ //if insufficient funds, reimburse
                accounts[account_id].balance = beginning_balance;
                return;
        }
        accounts[account_id].transaction_count += 1;
        return;
}

void transfer(int incoming_account_id, int outgoing_account_id, int amount) {
        int incoming_beginning_balance = accounts[incoming_account_id].balance;
        accounts[incoming_account_id].balance += amount;  //add amount
        accounts[incoming_account_id].balance -= accounts[incoming_account_id].transfer_fee; //deduct fee
        if(accounts[incoming_account_id].transaction_count >= accounts[incoming_account_id].transaction_cap){
                accounts[incoming_account_id].balance -= accounts[incoming_account_id].excess_fee; //deduct fee if required
        }
        int incoming_ending_balance = accounts[incoming_account_id].balance;
        //compute overdraft
        int incoming_overdrafted_fee = overdraft(accounts[incoming_account_id].overdraft_applied, incoming_beginning_balance, incoming_ending_balance, accounts[incoming_account_id].overdraft_fee);
        if(incoming_overdrafted_fee == -5001){ //if insufficient funds, reimburse
                accounts[incoming_account_id].balance = incoming_beginning_balance;
                return;
        }
        accounts[incoming_account_id].balance -= incoming_overdrafted_fee; //deduct fee
        if((accounts[incoming_account_id].balance < 0)&&(accounts[incoming_account_id].overdraft_boolean == 0)){ //if insufficient funds, reimburse
                accounts[incoming_account_id].balance = incoming_beginning_balance;
                return;
        }
        if(accounts[incoming_account_id].balance < -5000){ //if insufficient funds, reimburse
                accounts[incoming_account_id].balance = incoming_beginning_balance;
                return;
        }
        accounts[incoming_account_id].transaction_count += 1;

        int outgoing_beginning_balance = accounts[outgoing_account_id].balance;
        accounts[outgoing_account_id].balance -= amount;  //deduct amount
        accounts[outgoing_account_id].balance -= accounts[outgoing_account_id].transfer_fee; //deduct fee
        if(accounts[outgoing_account_id].transaction_count >= accounts[outgoing_account_id].transaction_cap){
                accounts[outgoing_account_id].balance -= accounts[outgoing_account_id].excess_fee; //deduct fee if required
        }
        int outgoing_ending_balance = accounts[outgoing_account_id].balance;
        //compute overdraft
        int outgoing_overdrafted_fee = overdraft(accounts[outgoing_account_id].overdraft_applied, outgoing_beginning_balance, outgoing_ending_balance, accounts[outgoing_account_id].overdraft_fee);
        if(outgoing_overdrafted_fee == -5001){ //if insufficient funds, reimburse
                accounts[outgoing_account_id].balance = outgoing_beginning_balance;
                accounts[incoming_account_id].balance = incoming_beginning_balance;
                return;
        }
        accounts[outgoing_account_id].balance -= outgoing_overdrafted_fee; //deduct fee
        if((accounts[outgoing_account_id].balance < 0)&&(accounts[outgoing_account_id].overdraft_boolean == 0)){ //if insufficient funds, reimburse
                accounts[outgoing_account_id].balance = outgoing_beginning_balance;
                accounts[incoming_account_id].balance = incoming_beginning_balance;
                return;
        }
        if(accounts[outgoing_account_id].balance < -5000){ //if insufficient funds, reimburse
                accounts[outgoing_account_id].balance = outgoing_beginning_balance;
                accounts[incoming_account_id].balance = incoming_beginning_balance;
                return;
        }
        accounts[outgoing_account_id].transaction_count += 1;

        return;
}

int overdraft(int already_applied, int beginning_balance, int ending_balance, int fee) {
        int bb = beginning_balance;
        int eb = ending_balance;
        int overdrafts = 0; //count of intervals of 500 (negative) that happened from beginning to end during transaction

        while (1) { // loop until breaking condition
                int beginning_overdrafts = 0; //no overdrafts at beginning (relative to end)
                if (bb < 0) { //if beginning balance was negative (possibility of overdraft)
                        double temp;
                        temp = (double)bb / 500; //see how many times balance goes into 500
                        temp = temp * -1; //make positive
                        temp = ceil(temp); //round up
                        beginning_overdrafts = (int)temp; //this is number of 500 intervals balance is under 0 at beginning
                }
                int ending_overdrafts = 0;
                if (eb < 0) { //if ending balance was negative (possibility of overdraft)
                        double temp;
                        temp = (double)eb / 500;  //see how many times balance goes into 500
                        temp = temp * -1; //make positive
                        temp = ceil(temp); //round up
                        ending_overdrafts = (int)temp; //this is number of 500 intervals balance is under 0 at end
                }

                int new_overdrafts = ending_overdrafts - beginning_overdrafts; //compare end intervals to beginning
                if (new_overdrafts == overdrafts) { //if the overdraft fee has not caused another overdraft fee to be needed
                        int overdrafts_to_apply = overdrafts - already_applied; //check if fee has already been applied to this level
                        return overdrafts_to_apply * fee; //return appropriate fee
                }
                overdrafts = new_overdrafts; //check if another fee must be applied
                eb = ending_balance - new_overdrafts * fee;
                if (eb < -5000) { //if under -5000 return flag
                        return -5001;
                }
        }
        return 0;
}