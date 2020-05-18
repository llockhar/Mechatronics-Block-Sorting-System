//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MILESTONE : Final Project
// PROGRAM : MECH 458
// PROJECT : Sorting Conveyor Belt
// GROUP : 12
// NAME 1 : Max Wardle
// NAME 2 : Lisette Lockhart
// DESC : This program sorts blocks on a conveyor
// DATA
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <avr/io.h>
#include <stdlib.h>
#include <util/delay_basic.h>
#include <avr/interrupt.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////// Type Definitions ////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
	char itemCode; 	// stores a number describing the element
                    // 1 = Black (default)
                    // 2 = Steel (metal default)
                    // 3 = White
                    // 4 = Aluminium

	char stage; 	
} element;

typedef struct link{
	element		e;
	struct link *next;
} link;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////// Global Variables ////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int BLACK = 1;
int STEEL = 2;
int WHITE = 3;
int ALUMINIUM = 4;

volatile unsigned int BLACK_REFL_LOW = 0b1110100110;
volatile unsigned int STEEL_REFL_LOW = 0b0100011001;

volatile unsigned int NUM_BLACK = 0;
volatile unsigned int NUM_STEEL = 0;
volatile unsigned int NUM_WHITE = 0;
volatile unsigned int NUM_ALUMINIUM = 0;
volatile unsigned int NUM_UNSORTED = 0;

volatile int STEPPER_POSITION = 1;                                    
volatile int STEP_NUMBER = 1;

volatile unsigned int ADC_RESULT = 0;
volatile unsigned int ADC_RESULT_FLAG = 0;
volatile unsigned int OP_2_RISING_EDGE = 1;
volatile unsigned int ADC_HL;

volatile unsigned int INIT_MODE = 0;
volatile unsigned int GO_MODE = 0;
volatile unsigned int STOP_FLAG = 0;


link *HEAD;     // used by Optical Sensor #3 
link *TAIL;     // used by Inductive Sensor
link *OPTICAL;  // used by ADC

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////// Initialization Functions ////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// initialize stepper position based on hall sensor
void initialize_position(){

	step_ccw(20, 20); // offset

	mTimer(20);
	
	// initialize to black
	while ((PIND & 0x01) == 0x01){
		PORTC = PIND;
		step_cw(1, 20, 0);
	}
	
	// initialize stepper position
	STEPPER_POSITION = BLACK;
}

void init_reflectivity(){

    start_dc_motor();

    INIT_MODE = 1;

    int num = 0;

    unsigned int temp = 0;
    unsigned int templ = 0;
    unsigned int temph = 0;
    
    PORTC = BLACK;

    while(num < 20){
        
        if(num == 10){
            PORTC = WHITE;
        }
        if (ADC_RESULT_FLAG){
            temp += ADC_HL;
            num++;
            ADC_RESULT_FLAG = 0;
        }
    }

    temp = temp/20;
    
    PORTC = temp;
    PORTD = 0x00;
    if ((temp & 0b100000000) == 0b100000000){PORTD |= 0b10000000;}
    if ((temp & 0b1000000000) == 0b1000000000){PORTD |= 0b00010000;}

}

void initialize_interrupts(){
	
	// disable all interrupts
	cli(); 

	// configure external interrupts
	EIMSK |= _BV(INT1); 				// enable INT1 (optical sensor 1)
	EICRA |= _BV(ISC11);                // falling edge interrupt

	EIMSK |= _BV(INT2); 				// enable INT2 (pause button)
	EICRA |= (_BV(ISC21) | _BV(ISC20)); // rising edge interrupt

    EIMSK |= _BV(INT3); 				// enable INT3 (inductive sensor)
	EICRA |= _BV(ISC31); 				// falling edge interrupt

    EIMSK |= _BV(INT4); 				// enable INT4 (optical sensor 2)
	EICRB |= _BV(ISC40);            	// any edge interrupt

    EIMSK |= _BV(INT5); 				// enable INT5 (optical sensor 3)
	EICRB |= _BV(ISC51);              	// falling edge interrupt

	EIMSK |= _BV(INT6); 				// enable INT6 (ramp down button)
	EICRA |= (_BV(ISC61) | _BV(ISC60)); // rising edge interrupt

	// configure ADC 
	ADCSRA |= _BV(ADEN); 				// enable ADC
	ADCSRA |= _BV(ADIE); 				// enable interrupt of ADC
    ADCSRA |= _BV(ADPS2);               // prescaler = fclk/64 for 8 MHz clock
    ADCSRA |= _BV(ADPS1);               
    ADCSRB |= 0b10000000;               // high speed mode
	ADMUX |= _BV(REFS0);	            // external cap
	ADMUX |= _BV(MUX0);					// channel 1 

	// set the Global Enable for all interrupts 
	sei();
	
}

// initialize the PWM port for the DC motor
void init_pwm(void)
{
	TCCR0A |= _BV(WGM01);	// set counting sequence waveform generation mode
	TCCR0A |= _BV(WGM00);	// set counting sequence waveform generation mode
							// both WGM01 and WGM00 are set to 1 so we are in fast PWM mode
	TCCR0A |= _BV(COM0A1);	// clear OCA on compare match, set OCA at top
	TCCR0B |= _BV(CS01);	// prescaler clk/8 1 MHz clock
    TCCR0B |= _BV(CS00);    // prescaler clk/64 8 MHz clock
	DDRB |= _BV(PB7);		// output on PORTB, pin 7
	OCR0A = 0x6F;			// set output compare register to compare on 127
                            // controls the motor speed
}

void initialize_ports(){

	DDRA = 0xFF;		// Port A will be used to drive the stepper motor
	
	DDRB = 0xFF;		// Port B will be used to drive the DC motor
						// PWM output on bit 7
						// IN A output on bit 2
						// IN B output on bit 1
						// Enable output on bit 0

	DDRC = 0xFF;		// Port C will be used do light the LED display
	
	DDRD = 0xF0;		// Port D will be used to handle interrupts
						// Proximity sensor on bit 3
						// Pause/Resume button on bit 2
						// Optical sensor 1 on bit 1
						// Hall effect sensor on bit 0

    DDRE = 0x00;		// Port E will be used to handle interrupts
						// Stop button on bit 6
						// Optical sensor 3 on bit 5
						// Optical sensor 2 on bit 4

                        // PORT F PIN 2 IS FOR ADC

}

void init_linked_list(link **h,link **t, link **o){
	*h = NULL;
	*t = NULL;		
    *o = NULL;
	return;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////// Interrupt Handlers //////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 1st Optical Sensor 
ISR(INT1_vect)
{ // Creates new link in linked list
    
    // when there is a rising edge, we need to add item to the list
    link *new_link = NULL;                      
    initLink(&new_link);             // create a new link
	new_link->e.itemCode = BLACK;    // initialize to BLACK plastic 
	new_link->e.stage = 0;
	enqueue(&HEAD, &TAIL, &OPTICAL, &new_link);
    ++NUM_UNSORTED;                  // increment number of unsorted items

}

// Pause/Resume Button
ISR(INT2_vect)
{ // Stops the belt and displays items sorted/unsorted, resumes operation

    mTimer(20);                         // debounce button

    if((PIND & 0b00000100) == 0b100){       // check if noise 

        while((PIND & 0b00000100) == 0b100);// debounce button

        stop_dc_motor();        // stop the belt
        GO_MODE = 0;            // indicate pause mode

        // if we were previously in normal operation then pause
        while (!GO_MODE){


            // display number of sorted and unsorted items
            PORTC = ((BLACK<<5) | NUM_BLACK);
            mTimerPollReset(1000);
            PORTC = ((STEEL<<5) | NUM_STEEL);
            mTimerPollReset(1000);
            PORTC = ((WHITE<<5) | NUM_WHITE);
            mTimerPollReset(1000);
            PORTC = ((ALUMINIUM<<5) | NUM_ALUMINIUM);
            mTimerPollReset(1000);
            PORTC = (0b11100000 | NUM_UNSORTED);
            mTimerPollReset(1000);

        }

        //if we were previously paused then resume
    
        PORTC = 0b00000000;     // clear display
        start_dc_motor();       // start the belt
        //GO_MODE = 1;            // indicate go mode
    }
}

// Inductive Sensor 
ISR(INT3_vect)
{ // changes itemCode for metallic material
    
    // this is a metal
    TAIL->e.itemCode = STEEL; // initialize to STEEL   

}

// 2nd Optical Sensor
ISR(INT4_vect)
{ // starts or ends ADC conversions 

    // If the part is first being detected start ADCs
    if(OP_2_RISING_EDGE){
            
        ADC_HL = 0x3FF;         //reset ADC values to max

        OP_2_RISING_EDGE = 0;   // do continuous ADC conversions

	    ADCSRA |= _BV(ADSC);    // start ADC conversion
          
    } else {
        
        OP_2_RISING_EDGE = 1;   // stop doing ADC conversions

    }
}

// 3rd Optical Sensor (end of belt)
ISR(INT5_vect)
{
        
    stop_dc_motor();    // move stepper

    PORTC = HEAD->e.itemCode;

    move_stepper();     // move stepper
    //start_dc_motor();   // start belt

    // update stepper position
    STEPPER_POSITION = HEAD->e.itemCode;

    // update number of sorted items
    if(HEAD->e.itemCode == BLACK){++NUM_BLACK;}
    else if(HEAD->e.itemCode == STEEL){++NUM_STEEL;}
    else if(HEAD->e.itemCode == WHITE){++NUM_WHITE;}
    else {++NUM_ALUMINIUM;}
    
    dequeue(&HEAD);     // discard first link of list
    --NUM_UNSORTED;     // update number of unsorted items

    // when Stop button is pushed, sort belt items then display
    if (STOP_FLAG){
        if(NUM_UNSORTED == 0){
            mTimer(2000);
            stop_display();
        }
    }
}

// Stop button
ISR(INT6_vect)
{ // finish sorting items then display amount of each type

    mTimer(20);                         // debounce button
    while((PINE & 0b01000000) == 0);    // debounce button
	int fin = mTimerPollStop(3000);     // debounce button

    STOP_FLAG = 1;  // indicate we need to ramp down

    // if there are no items on belt then display
    if (NUM_UNSORTED == 0 && fin){
        stop_display();
    }
}

// ADC
ISR(ADC_vect)
{// the interrupt will be triggered if the ADC is done

   if(!OP_2_RISING_EDGE)
   {// get lowest value
        int  temp = ADC;        // temporary ADC result storage

        // get lowest ADC value
        if(temp < ADC_HL ){
            ADC_HL = temp;
        }

        //PORTC = temp;
        ADCSRA |= _BV(ADSC);    // start another ADC

    }
    else
    {// display and store values
        
        // during initialization
        if(INIT_MODE){            
            ADC_RESULT_FLAG = 1;
        } else {

        	// classify material
        	if(OPTICAL->e.itemCode == STEEL){			//if metal
        		if(ADC_HL < STEEL_REFL_LOW){
    				OPTICAL->e.itemCode = ALUMINIUM;
        		}
        	} else if(OPTICAL->e.itemCode == BLACK){	//if plastic
        		if(ADC_HL < BLACK_REFL_LOW){
    				OPTICAL->e.itemCode = WHITE;
        		}
        	}

        	OPTICAL = OPTICAL->next;   // point to next item in list

        }
    }
}

// this interrupt will be triggered if no interrupt handler for interrupt available
ISR(BADISR_vect){
	// do some LED pattern here
	PORTC = 0b1011101;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////// General Purpose Functions ///////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// times in milliseconds given input
void mTimer (int count){

	int i = 0;

	TCCR1B |= _BV(WGM12);  // timer counter control, Wave Genration Mode 12, Clear Timer on Compare (CTC) **

	//OCR1A = 0x03E8;      // output compare 1 MHz clock
    OCR1A = 0x1F40;        // output compare 8 MHz clock

	TCNT1 = 0x0000;        // set timer counter to 0 initially

	TIFR1 |= _BV(OCF1A);   // timer interrupt flag register, bit value, clear flag **
 
	while (i<count){
		if((TIFR1 & 0x02) == 0x02){
			TIFR1 |= _BV(OCF1A);
			++i;
		}
	}
	return;
}

void mTimerPollReset(int num){
    for (int i = 0; i < num; i++){
        mTimer(1);
        if((PIND & 0b00000100) == 0b100 && GO_MODE == 0){
            GO_MODE = 1;
        }
    }
}

int mTimerPollStop(int num){
    for (int i = 0; i < num; i++){
        mTimer(1);
        if((PIND & 0b00000010) != 0b010){
        
            // when there is a rising edge, we need to add item to the list
            //link *new_link = NULL;                      
            //initLink(&new_link);             // create a new link
            //new_link->e.itemCode = BLACK;    // initialize to BLACK plastic 
            //new_link->e.stage = 0;
            //enqueue(&HEAD, &TAIL, &OPTICAL, &new_link);
            //++NUM_UNSORTED; // increment number of unsorted items
            return 0;
        }
    }
    return 1;
}

void stop_display(){

    stop_dc_motor();
    while (1){
        PORTC = ((BLACK<<5) | NUM_BLACK);
        mTimer(1000);
        PORTC = ((STEEL<<5) | NUM_STEEL);
        mTimer(1000);
        PORTC = ((WHITE<<5) | NUM_WHITE);
        mTimer(1000);
        PORTC = ((ALUMINIUM<<5) | NUM_ALUMINIUM);
        mTimer(1000);
    }

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////// Stepper Motor Functions /////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void move_stepper(){
    
    int max_delay = 20; // 20 ms lowest speed delay
	int min_delay = 6;  // 6 ms top speed delay
	int num_steps_90_deg = 50;
	int num_steps_180_deg = 100;

    // check where stepper is located then where it needs to go
    if(STEPPER_POSITION == BLACK){
        if(HEAD->e.itemCode == STEEL){
            step_ccw(num_steps_90_deg, max_delay, min_delay); // 90 deg counter clockwise
        } else if(HEAD->e.itemCode == WHITE){
            step_ccw(num_steps_180_deg, max_delay, min_delay); // 180 deg counter clockwise
        } else if(HEAD->e.itemCode == ALUMINIUM){
            step_cw(num_steps_90_deg, max_delay, min_delay); // 90 deg clockwise
        } else {
            start_dc_motor();
        }
    }else if(STEPPER_POSITION == STEEL){
        if(HEAD->e.itemCode == BLACK){
            step_cw(num_steps_90_deg, max_delay, min_delay); // 90 deg clockwise
        } else if(HEAD->e.itemCode == WHITE){
            step_ccw(num_steps_90_deg, max_delay, min_delay); // 90 deg counter clockwise
        } else if(HEAD->e.itemCode == ALUMINIUM){
            step_cw(num_steps_180_deg, max_delay, min_delay); // 180 deg clockwise
        } else {
            start_dc_motor();
        }
    }else if(STEPPER_POSITION == WHITE){
        if(HEAD->e.itemCode == STEEL){
            step_cw(num_steps_90_deg, max_delay, min_delay); // 90 deg clockwise
        } else if(HEAD->e.itemCode == BLACK){
            step_ccw(num_steps_180_deg, max_delay, min_delay); // 180 deg counter clockwise
        } else if(HEAD->e.itemCode == ALUMINIUM){
            step_ccw(num_steps_90_deg, max_delay, min_delay); // 90 deg counter clockwise
        } else {
            start_dc_motor();
        }
    }else if(STEPPER_POSITION == ALUMINIUM){
        if(HEAD->e.itemCode == STEEL){
            step_ccw(num_steps_180_deg, max_delay, min_delay); // 180 deg counter clockwise
        } else if(HEAD->e.itemCode == WHITE){
            step_cw(num_steps_90_deg, max_delay, min_delay); // 90 deg clockwise
        } else if(HEAD->e.itemCode == BLACK){
            step_ccw(num_steps_90_deg, max_delay, min_delay); // 90 deg counter clockwise
        } else {
            start_dc_motor();
        }
    }
}


void step_once_cw(){

	char step1 = 0b00110110;
	char step2 = 0b00101110;
	char step3 = 0b00101101;
	char step4 = 0b00110101;

	if(STEP_NUMBER == 1){

		PORTA = step2;
		STEP_NUMBER = 2;

	} else if(STEP_NUMBER == 2){

		PORTA = step3;
		STEP_NUMBER = 3;

	} else if(STEP_NUMBER == 3){

		PORTA = step4;
		STEP_NUMBER = 4;

	} else if(STEP_NUMBER == 4){

		PORTA = step1;
		STEP_NUMBER = 1;
	}
}

void step_once_ccw(){

	char step1 = 0b00110110;
	char step2 = 0b00101110;
	char step3 = 0b00101101;
	char step4 = 0b00110101;

	if(STEP_NUMBER == 3){

		PORTA = step2;
		STEP_NUMBER = 2;

	}else if(STEP_NUMBER == 4){

		PORTA = step3;
		STEP_NUMBER = 3;

	}else if(STEP_NUMBER == 1){

		PORTA = step4;
		STEP_NUMBER = 4;

	}else if(STEP_NUMBER == 2){

		PORTA = step1;
		STEP_NUMBER = 1;
	}
}


void step_cw(int num_steps, int delay_ms, int delay_min){

	for(int i = 0; i < num_steps; ++i){
		
		step_once_cw();		// step once

		mTimer(delay_ms);	// delay

        // accelerate for 90 degree turn
		if(num_steps == 50){
            if(i < 2) {--delay_ms;}
			else if(delay_ms > delay_min){delay_ms-=2;}
            if(i > 47){++delay_ms;}
			else if(i > (num_steps - (2 * delay_min))){delay_ms+=2;}
            if(i == 30){start_dc_motor();} //start DC motor before stepper reaches position
		}
        // accelerate for 180 degree turn
		if(num_steps == 100){
            if(i < 2) {--delay_ms;}
			else if(delay_ms > delay_min){delay_ms-=2;}
            if(i > 87){++delay_ms;}
			else if(i > (num_steps - (2 * delay_min))){delay_ms+=2;}
            if(i == 80){start_dc_motor();} //start DC motor before stepper reaches position
		}
	}
}

void step_ccw(int num_steps, int delay_ms, int delay_min){

	int min_delay = 10;

	for(int i = 0; i < num_steps; ++i){

		step_once_ccw();
		
		mTimer(delay_ms);	// delay

		// accelerate for 90 degree turn
		if(num_steps == 50){
            if(i < 2) {--delay_ms;}
			else if(delay_ms > delay_min){delay_ms-=2;}
            if(i > 47){++delay_ms;}
			else if(i > (num_steps - (2 * delay_min))){delay_ms+=2;}
            if(i == 30){start_dc_motor();} //start DC motor before stepper reaches position
		}
        // accelerate for 180 degree turn
		if(num_steps == 100){
            if(i < 2) {--delay_ms;}
			else if(delay_ms > delay_min){delay_ms-=2;}
            if(i > 87){++delay_ms;}
			else if(i > num_steps - (2 * delay_min)){delay_ms+=2;}
            if(i == 80){start_dc_motor();} //start DC motor before stepper reaches position
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////// DC Motor Functions //////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void start_dc_motor(){
	PORTB = 0b00000010;			// rotate ccw
}

void stop_dc_motor(){
	PORTB = 0b00000000;			// brake
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////// Data Storage Functions //////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// This initializes a link and returns the pointer to the new link or NULL if error 
void initLink(link **newLink){
	//link *l;
	*newLink = malloc(sizeof(link));
	(*newLink)->next = NULL;
	return;
}

// This will put an item at the tail of the queue 
void enqueue(link **h, link **t, link **o, link **nL){

	if (*t != NULL){
		// Not an empty queue
		(*t)->next = *nL;
		*t = *nL; //(*t)->next;
	}
	else{
		// It's an empty Queue
		//(*h)->next = *nL;
		//should be this
		*h = *nL;
		*t = *nL;
        *o = *nL;
	}
	return;
}

// This will remove the link and element within the link from the head of the queue
void dequeue(link **h){
	link *deQueuedLink = *h;        // Will set to NULL if Head points to NULL
	
	if(size(&HEAD, &TAIL) == 1){    // Check that the list doesn't only have one item
        HEAD = NULL;
        TAIL = NULL;
        OPTICAL = NULL;
    }else if (*h != NULL){          // Ensure it is not an empty queue
		*h = (*h)->next;
	}

	//free(*deQueuedLink);

	return;
}

// This simply allows you to peek at the head element of the queue and returns a NULL pointer if empty
element firstValue(link **h){
	return((*h)->e);
}

// This clears the queue
void clearQueue(link **h, link **t){

	link *temp;

	while (*h != NULL){
		temp = *h;
		*h=(*h)->next;
		free(temp);
	}

	*t = NULL;		

	return;
}

// Check to see if the queue is empty 
char isEmpty(link **h){
	return(*h == NULL);
}

// Returns the size of the queue
int size(link **h, link **t){

	link 	*temp;			// will store the link while traversing the queue
	int 	numElements;

	numElements = 0;

	temp = *h;			    // point to the first item in the list

	while(temp != NULL){
		numElements++;
		temp = temp->next;
	}	
	return(numElements);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////// Main Function ///////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void main(){

    CLKPR = 0b10000000;
    CLKPR = 0b00000000;                         // set clock speed to 8 MHz
	
	TCCR1B|=_BV(CS10);			                // clock select

	initialize_ports();			                // initialize the I/O port directions
		
	PORTB = 0b0000;				                // initialize to break DC motor

	initialize_position();		                // initialize the stepper position

    init_linked_list(&HEAD, &TAIL, &OPTICAL);   // initalize the linked list

	initialize_interrupts();    	            // Initialize the interrupts

	init_pwm();				    	            // initialize the PWM

	start_dc_motor();		    	            // start DC motor

    GO_MODE = 1;                                // set mode

    //init_reflectivity();                      // initialize reflectivity if items for reflective sensor (only use for calibration)

	// main loop
	while(1){

	// wait for interrupts
        
	}
	return;
}
