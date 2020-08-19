/*
 * This is a high speed data logger. It acquires data and stores it on an SD 
 * card. In my tests using a Teensy 3.6 and a SanDisk Ultra 16GB SDHC micro SD 
 * card, I was able to sample an analog pin at 25 kHz.
 * 
 * It relies on the beta version of the SdFat library written by Bill Greiman,
 * which is available at https://github.com/greiman/SdFat-beta. I have tested
 * this sketch with revision ffbccb0 of SdFat-beta. This code was inpired by 
 * SdFat's LowLatencyLogger and TeensySdioDemo examples. It uses the same
 * binary file format as LowLatencyLogger, but with a bigger block size.
 * 
 * Here is how the code works. We have four buffers that are used to store the
 * data. The main loop is very simple: it checks to see if there are any full 
 * buffers, and if there are, it writes them to the SD card. When is the data 
 * acquired? Data is acquired in the function yield(), which is called 
 * whenever the Teensy is not busy with something else. yield() is called by 
 * the main loop whenever there is nothing to write to the SD card. yield() is 
 * also called by the SdFat library whenever it is waiting for the SD card.
 */

// if true, will print to serial monitor. disable if not using serial monitor
bool print_serial = true;
bool print_serial_data = false;

// buffer and file sizes
const size_t BUF_DIM = 16384;
const uint32_t fileSizeWrite = 262144;

// include libraries
#include <SdFat.h>
#include <TinyGPS.h>
#include <Chrono.h>

Chrono SyncTimer;

// Pin with LED, which flashes whenever data is written to card, and does a 
// slow blink when recording has stopped.
const int LED_PIN = 13;

// Use total of four buffers.
const uint8_t BUFFER_BLOCK_COUNT = 4;

//GPS stuff
TinyGPS gps;
float flat = 0.0;
float flon = 0.0; 
uint32_t fix_age = 0;
uint32_t gps_time = 0;
uint32_t date = 0;
uint32_t age = 0; 
char gps_raw_data;
uint8_t num_satellites;
String tmp;
bool gps_fresh;

// Format for one data record
struct data_t {
  uint32_t time;
  float gps_lat;
  float gps_lon;
  uint32_t gps_time;
  uint32_t gps_date;
  char wind[128]; 
};
// Warning! The Teensy allocates memory in chunks of 4 bytes!
// sizeof(data_t) will always be a multiple of 4. For example, the following
// data record will have a size of 12 bytes, not 9:
// struct data_t {
//   uint32_t time; // 4 bytes
//   uint8_t  adc[5]; // 5 bytes
// }

// Use SdFatSdio (not SdFatSdioEX) because SdFatSdio spends more time in
// yield(). For more information, see the TeensySdioDemo example from the 
// SdFat library. 
SdFatSdio sd;

// File stuff
File file;
uint32_t dirname;
char dirname_char[8];
uint32_t fname = 1;
char fname_char[8];
char full_fname_char[20];


// Number of data records in a block.
const uint16_t DATA_DIM = (BUF_DIM - 4)/sizeof(data_t);

//Compute fill so block size is BUF_DIM bytes.  FILL_DIM may be zero.
const uint16_t FILL_DIM = BUF_DIM - 4 - DATA_DIM*sizeof(data_t);

// Format for one block of data
struct block_t {
  uint16_t count;
  uint16_t overrun;
  data_t data[DATA_DIM];
  uint8_t fill[FILL_DIM];
};

//Buffer stuff -----------------------------------------------------------------------

// Intialize all buffers
block_t block[BUFFER_BLOCK_COUNT];

// Initialize full queue
const uint8_t QUEUE_DIM = BUFFER_BLOCK_COUNT + 1;

// Index of last queue location.
const uint8_t QUEUE_LAST = QUEUE_DIM - 1;

block_t* curBlock = 0;

block_t* emptyStack[BUFFER_BLOCK_COUNT];
uint8_t emptyTop;
uint8_t minTop;

block_t* fullQueue[QUEUE_DIM];
uint8_t fullHead = 0;
uint8_t fullTail = 0;

bool fileIsClosing = false;
bool collectingData = false;
bool isSampling = false;
bool justSampled = false;


//-----------------------------------------------------------------------------
void setup() {
  if (print_serial) {
    //Serial.begin(9600);
    //while (!Serial) {
  }

  Serial3.begin(115200);
  Serial3.flush();

  Serial4.begin(9600);// Baud rate of GPS (GP20U7)
  Serial4.flush();

  pinMode(LED_PIN, OUTPUT);

  // Put all the buffers on the empty stack.
  for (int i = 0; i < BUFFER_BLOCK_COUNT; i++) {
    emptyStack[i] = &block[i - 1];
  }
  emptyTop = BUFFER_BLOCK_COUNT;

  sd.begin();
  
  // every time teensy is rebooted, make a new directory
  getDirName();

  // get a new filename
  openNewFile();

  if (print_serial) {
    Serial.print("Block size: ");
    Serial.println(BUF_DIM);
    Serial.print("Record size: ");
    Serial.println(sizeof(data_t));
    Serial.print("Records per block: ");
    Serial.println(DATA_DIM);
    Serial.print("Record bytes per block: ");
    Serial.println(DATA_DIM*sizeof(data_t));
    Serial.print("Fill bytes per block: ");
    Serial.println(FILL_DIM);
    Serial.println("Recording. Enter any key to stop.");
  }
  
  delay(100);
  collectingData=true;
}
//-----------------------------------------------------------------------------
void loop() {
  
  if (SyncTimer.hasPassed(20000)){
    SyncTimer.restart();
    file.sync(); 

    if (print_serial) {
      Serial.print("File sync.");
      Serial.print(" GPS date: ");
      Serial.print(date);
      Serial.print(" GPS time: ");
      Serial.println(gps_time);
    }
  }

  if (file.size()>=fileSizeWrite) {
    fileIsClosing = true;
  }

  if (fileIsClosing) {
    file.close();

    if (print_serial) {
      Serial.print("File complete.");
      Serial.print(" GPS date: ");
      Serial.print(date);
      Serial.print(" GPS time: ");
      Serial.println(gps_time);
    }
    
    // open new file
    fileIsClosing = false;
    openNewFile();
  }

  // Write the block at the tail of the full queue to the SD card
  if (fullHead == fullTail) { // full queue is empty
    yield();
    
  } else { // full queue not empty
    // write buffer at the tail of the full queue and return it to the top of
    // the empty stack.
    digitalWrite(LED_PIN, HIGH);
    block_t* pBlock = fullQueue[fullTail];
    fullTail = fullTail < QUEUE_LAST ? fullTail + 1 : 0;
    if ((int)BUF_DIM != file.write(pBlock, BUF_DIM)) {
      error("write failed");
    }
    else {
      emptyStack[emptyTop++] = pBlock;
      digitalWrite(LED_PIN, LOW);
    }
  }


  fileIsClosing = Serial.available();
  
}
//-----------------------------------------------------------------------------
void yield(){
  // This does the data collection. It is called whenever the teensy is not
  // doing something else. The SdFat library will call this when it is waiting
  // for the SD card to do its thing, and the loop() function will call this
  // when there is nothing to be written to the SD card.

  if (!collectingData || isSampling)
    return;

  isSampling = true;

  // If file is closing, add the current buffer to the head of the full queue
  // and skip data collection.
  if (fileIsClosing) {
    collectingData = true;
    isSampling = false;
    return;
  }
  
  // If we don't have a buffer for data, get one from the top of the empty 
  // stack.
  if (curBlock == 0) {
    curBlock = getEmptyBlock();
  }

  while (Serial4.available()) {
      gps_raw_data = Serial4.read();
      gps.encode(gps_raw_data);
      gps.f_get_position(&flat, &flon, &age);
      gps.get_datetime(&date, &gps_time, &fix_age); // time in hhmmsscc, date in ddmmyy
  }

  // If it's time, record one data sample.
  if (Serial3.available()) {
        acquireData(&curBlock->data[curBlock->count++]);
  }

  // If the current buffer is full, move it to the head of the full queue. We
  // will get a new buffer at the beginning of the next yield() call.
  if (curBlock->count == DATA_DIM) {
    putCurrentBlock();
  }

  isSampling = false;
}
//-----------------------------------------------------------------------------
block_t* getEmptyBlock() {
  /*
   * Takes a block form the top of the empty stack and returns it
   */
  block_t* blk = 0;
  if (emptyTop > 0) { // if there is a buffer in the empty stack
    blk = emptyStack[--emptyTop];
    blk->count = 0;
  } else { // no buffers in empty stack
    error("All buffers in use");
  }
  return blk;
}
//-----------------------------------------------------------------------------
void putCurrentBlock() {
  /*
   * Put the current block at the head of the queue to be written to card
   */
  fullQueue[fullHead] = curBlock;
  fullHead = fullHead < QUEUE_LAST ? fullHead + 1 : 0;
  curBlock = 0;
}
//-----------------------------------------------------------------------------
void error(String msg) {
  if (print_serial) {
    Serial.print("ERROR: ");
    Serial.println(msg);
  }
  //blinkForever();
}
//-----------------------------------------------------------------------------
void acquireData(data_t* data){
  data->time = millis();
  tmp = Serial3.readStringUntil('\r');
  tmp.toCharArray(data->wind, 128);

  data->gps_lat = flat;
  data->gps_lon = flon;
  data->gps_date = date;
  data->gps_time = gps_time;

  if (print_serial_data) {
    Serial.print(data->time);
    Serial.print(":  ");
    Serial.print(data->gps_date);
    Serial.print(":  ");
    Serial.print(data->gps_time);
    Serial.print(":  ");
    Serial.println(data->wind);
  }
  
}

//-----------------------------------------------------------------------------
void blinkForever() {
  while (1) {
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    digitalWrite(LED_PIN, LOW);
    delay(1000);
  }
}

void getFullFname() {
  // every time teensy is rebooted, make a new directory
  sprintf(fname_char, "%07lu", fname);
  strcpy(full_fname_char, dirname_char);
  strcat(full_fname_char, "/");
  strcat(full_fname_char, fname_char);
  strcat(full_fname_char, ".bin");

  while (sd.exists(full_fname_char)){

    if (print_serial) {
      Serial.print("fname exists: ");
      Serial.println(full_fname_char);
    }
    
    fname ++;
    Serial.print("trying fname number: ");
    Serial.println(fname);
    sprintf(fname_char, "%07lu", fname);
    strcpy(full_fname_char, dirname_char);
    strcat(full_fname_char, "/");
    strcat(full_fname_char, fname_char);
    strcat(full_fname_char, ".bin");
  }
}

void openNewFile() {
  getFullFname();
  if (print_serial) {
      Serial.print("Opening file for writing: ");
      Serial.println(full_fname_char);
  }
  if (!file.open(full_fname_char, O_RDWR | O_CREAT)) {
    error("open failed");
  }
  else {
    if (print_serial) {
      Serial.print("Opened file for writing: ");
      Serial.println(full_fname_char);
    }
  }
}

void getDirName() {
  dirname = 1;
  sprintf(dirname_char, "%07lu", dirname);
  while (sd.exists(dirname_char)){
    if (print_serial) {
      Serial.print("dirname exists: ");
      Serial.println(dirname_char);
    }
    dirname ++;
    sprintf(dirname_char, "%07lu", dirname);
  }
  sd.mkdir(dirname_char);
  if (print_serial) {
    Serial.print("made directory: ");
    Serial.println(dirname_char);
  }
}
