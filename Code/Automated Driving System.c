#include <Arduino.h>
#include <avr/io.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#define STACK_SIZE 200
#define ledBrake 10
#define ledY1 6
#define ledY2 7
#define ledY3 8

#define PUSH_B1 2
#define PUSH_B2 3
#define SPEAKER 9
#define PTTM 0

unsigned long previousTime = 0;
volatile int brake_flag = 0;
volatile int current_speed = 0;
volatile int desired_speed = 0;

unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
unsigned long debounceDelay1 = 500;
unsigned long debounceDelay2 = 500;

QueueHandle_t xQueueUART = 0;
QueueHandle_t xQueueSafeDist = 0;

//remap potentiometer from [0-1023] to [0-255]
int remapDist(int value){
	return (int) (value * 255.0 / 1023.0);
}

//indicates safe brake
void brakeFlag(void){
	TickType_t xCurrTime = xTaskGetTickCount();
	if (brake_flag == 1){

		if (xCurrTime - previousTime >= 1000){
			previousTime = xCurrTime;

			digitalWrite(ledBrake, HIGH);

		}
		brake_flag = 0;
	}
}

void int0ISR(void){

	if((millis() - lastDebounceTime1) > debounceDelay1){
		if(current_speed < 3){
			current_speed++;
		}
		desired_speed = current_speed;
		lastDebounceTime1 = millis();
	}

}

void int1ISR(){

	if((millis() - lastDebounceTime2) > debounceDelay2){
		if(current_speed > 0){
			current_speed--;
		}
		desired_speed = current_speed;
		lastDebounceTime2 = millis();
	}
}

//decreases current speed after safe brake and get the minimum value in the pair of desired speed and safe speed
void setSafeSpeed(int dist){
	if( dist == 1){
		current_speed = 0;
	}
	else if(dist == 2){
		if(desired_speed > 0)
			current_speed = 1;
	}
	else if(dist == 3){
		if(desired_speed == 3)
			current_speed = 2;
	}
	else {
		if(desired_speed < 3){
			current_speed = desired_speed;
		}
		else{
			current_speed = 3;
		}
	}
}

void speed_task(void *p){

	TickType_t xPrevTime;
	const TickType_t xPeriod = 1000;

	while(1){

		int safeDistance;
		xPrevTime = xTaskGetTickCount();


		xQueueReceive(xQueueSafeDist, &safeDistance, portMAX_DELAY);
		digitalWrite(ledBrake, LOW);
		if((safeDistance < 2) && (current_speed == 1)){
			brake_flag = 1;
			digitalWrite(ledBrake, LOW);
			brakeFlag();
		}
		else if((safeDistance < 3) && (current_speed == 2)){
			brake_flag = 1;
			digitalWrite(ledBrake, LOW);

			brakeFlag();
		}
		else if((safeDistance < 4) && (current_speed == 3)){
			brake_flag = 1;

			digitalWrite(ledBrake, LOW);

			brakeFlag();
		}
		setSafeSpeed(safeDistance);


		if (current_speed == 0){
			digitalWrite(ledY1, LOW);
			digitalWrite(ledY2, LOW);
			digitalWrite(ledY3, LOW);
	//		digitalWrite(ledBrake, LOW);
			tone(SPEAKER, 1519);
		}

		else if (current_speed == 1){

			digitalWrite(ledY1, HIGH);
			digitalWrite(ledY2, LOW);
			digitalWrite(ledY3, LOW);
		//	digitalWrite(ledBrake, LOW);
			tone(SPEAKER, 1432);

		}

		else if (current_speed == 2){

			digitalWrite(ledY1, HIGH);
			digitalWrite(ledY2, HIGH);
			digitalWrite(ledY3, LOW);
			tone(SPEAKER, 1136 );
	//		digitalWrite(ledBrake, LOW);
//			if(safeDistance < 3){
//				brake_flag = 1;
//
//				digitalWrite(ledBrake, LOW);
//				//setSafeSpeed(safeDistance);
//				brakeFlag();
//			}

		}

		else if (current_speed == 3){

			digitalWrite(ledY1, HIGH);
			digitalWrite(ledY2, HIGH);
			digitalWrite(ledY3, HIGH);
			tone(SPEAKER, 956);

//			digitalWrite(ledBrake, LOW);
//			if(safeDistance < 4){
//				brake_flag = 1;
//
//				digitalWrite(ledBrake, LOW);
//				//setSafeSpeed(safeDistance);
//				brakeFlag();
//			}

		}

		vTaskDelayUntil(&xPrevTime, xPeriod);
	}

}

void distance_task(void *p) {
	int distance = 0;
	TickType_t xPrevTime;
	const TickType_t xPeriod = 1000;
	xPrevTime = xTaskGetTickCount();
	while(1){
		int value;
		value = analogRead(PTTM);
		value = remapDist(value);
		if (value <= 64){
			distance = 1;

		}
		else if ((value > 64) && (value <= 128)){
			distance = 2;

		}
		else if ((value > 128) && (value <= 192)){
			distance = 3;
		}
		else if (value > 192){
			distance = 4;
		}
		//Sends safe distance to speed Task
		xQueueSend(xQueueSafeDist, &distance, portMAX_DELAY);
		//Sends safe distance to serialPrint
		xQueueSend(xQueueUART, &distance, portMAX_DELAY);

		vTaskDelayUntil(&xPrevTime, xPeriod);
	}

}

//Prints information on UART
void serialPrint(void *p){
	while(1){
		int taskDistance;
		xQueueReceive(xQueueUART, &taskDistance, portMAX_DELAY);
		Serial.print("Desired Speed: ");
		Serial.println(desired_speed);
		Serial.print("Current Speed: ");
		Serial.println(current_speed);
		Serial.print("Distance: ");
		if( taskDistance != 1)
			Serial.print(taskDistance);
		Serial.println("d");
	}
}


void setup() {
	Serial.begin(115200);
	pinMode(ledBrake, OUTPUT);
	pinMode(ledY1, OUTPUT);
	pinMode(ledY2, OUTPUT);
	pinMode(ledY3, OUTPUT);
	pinMode(SPEAKER, OUTPUT);
	pinMode(PUSH_B1, INPUT);
	pinMode(PUSH_B2, INPUT);
	attachInterrupt(0,int0ISR, RISING);
	attachInterrupt(1,int1ISR, RISING);
}

void loop() {
	//create message queues
	xQueueUART = xQueueCreate(10, sizeof(unsigned long));
	xQueueSafeDist = xQueueCreate(10, sizeof(unsigned long));

	//create tasks
	xTaskCreate(speed_task, "Task1", STACK_SIZE, NULL, 3, NULL);

	xTaskCreate(distance_task, "Task2", STACK_SIZE,  NULL, 2, NULL);

	xTaskCreate(serialPrint, "Task3", STACK_SIZE,  NULL, 1, NULL);

	vTaskStartScheduler();
}