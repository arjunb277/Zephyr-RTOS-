#include <zephyr.h>
//#include <board.h>
#include <version.h>
#include <device.h>
#include <gpio.h>
#include <pwm.h>
#include <misc/util.h>
#include <misc/printk.h>
#include <pinmux.h>
#include "../boards/x86/galileo/board.h"
#include "../boards/x86/galileo/pinmux_galileo.h"
#include "../drivers/gpio/gpio_dw_registers.h"
#include "../drivers/gpio/gpio_dw.h"
#include <shell/shell.h>

#define EDGE_RISING	(GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH)
#define PULL_UP 	0	/* change this to enable pull-up/pull-down */

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority of threads */
#define PRIORITY_A 5

#define PRIORITY 7

#define PRIORITY_B 10

/* sleep in ms */
#define SLEEPTIME 1000

K_SEM_DEFINE(threadA_sem, 0, 1); /* starts off "not available" */
K_SEM_DEFINE(threadB_sem, 1, 1); /* starts off "available" */
K_MUTEX_DEFINE(my_mutex);/* mutex for message passing */

/* Thread initialization */
K_THREAD_STACK_DEFINE(threadB_stack_area,STACKSIZE);
K_THREAD_STACK_DEFINE(threadA_stack_area,STACKSIZE);
K_THREAD_STACK_DEFINE(tx_thread_stack_area,STACKSIZE);
K_THREAD_STACK_DEFINE(rx_thread_stack_area,STACKSIZE);
static struct k_thread threadA_data;
static struct k_thread threadB_data;
static struct k_thread tx_thread_data;
static struct k_thread rx_thread_data;

/* GPIO and PWM initialization */
static struct device *pinmux;
static struct gpio_callback gpio_cb;
static struct gpio_callback gpio_cb_1;
struct device *gpiob, *pwm0;
int ret,msg,start_time,stop_time;
int give_A,take_A,give_B;

static int count = 0; /* sample count */
int measure1_buff[100] = {0}; /* buffer for measurement 1 */
int measure2_buff[101] = {0}; /* buffer for measurement 2 */
int measure3_buff[100] = {0}; /* buffer for measurement 3 */ 

struct data_item_type {
      u32_t field1;
      u32_t field2;
      u32_t field3;
};

k_tid_t tid_a; /* thread A id */
k_tid_t tid_b; /* thread B id */
   
char __aligned(4) my_msgq_buffer[12 * sizeof(struct data_item_type)];
struct k_msgq my_msgq;

void interrupt_cb(struct device *gpiob, struct gpio_callback *cb, u32_t pins) /* interrupt callback for IO3 */
{
  //take time stamp
  stop_time = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32());
  measure1_buff[count] = (stop_time - start_time);
  //printk("Interrupt arrived %d\n", count+1);
  ++count;
}

void interrupt_cb_1(struct device *gpiob, struct gpio_callback *cb, u32_t pins) /* interrupt callback for IO2 */
{
  //take time stamp
  stop_time = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32());
  measure2_buff[count] = stop_time;
  //printk("Interrupt arrived at two %d\n", count+1);
  ++count;
}

/* lower priority thread for context switch */
void threadB( void *dummy1, void *dummy2, void *dummy3)
{
  ARG_UNUSED(dummy1);
  ARG_UNUSED(dummy2);
  ARG_UNUSED(dummy3);
  
  while(count<100)
  {
    k_sleep(20);
    k_sem_take(&threadB_sem, K_FOREVER); 

    /* say hello */
    //printk("Hi from thread B\n");
 
    give_A = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32());
    k_sem_give(&threadA_sem);
  }
return;
}

/*higher priority thread for context switch */
void threadA( void *dummy1, void *dummy2, void *dummy3)
{
  ARG_UNUSED(dummy1);
  ARG_UNUSED(dummy2);
  ARG_UNUSED(dummy3);

  /*create thread B */
  tid_b = k_thread_create(&threadB_data,threadB_stack_area,STACKSIZE, threadB, NULL, NULL, NULL, PRIORITY_B, 0, K_NO_WAIT);      
  k_thread_name_set(tid_b, "thread_b");

  while(count < 100)
  {
    k_sem_take(&threadA_sem, K_FOREVER);
    take_A = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32());

    /* say hello */
    //printk("Hi from thread A\n");
    
    //give_B1 = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32());
    k_sem_give(&threadB_sem);
    give_B = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32());

    int val = (take_A - give_A) - (give_B - take_A);
    measure3_buff[count] = val;
    ++count;
  }
return;
}
/* producer thread for message passing */
void tx_thread( void *dummy1, void *dummy2, void *dummy3)
{
  ARG_UNUSED(dummy1);
  ARG_UNUSED(dummy2);
  ARG_UNUSED(dummy3);

  printk("Background thread started\n");

  struct data_item_type data;

  while(count < 101)
  {
    if (k_mutex_lock(&my_mutex, K_FOREVER) != 0){ //mutex lock
         printk("Mutex not locked\n");
    } 
    
    data.field1 = ++msg; // set data value
   
    while (k_msgq_put(&my_msgq, &data, K_NO_WAIT) != 0) { // send data
            /* message queue is full: purge old data & try again */
            k_msgq_purge(&my_msgq);
        }

    //printk("Posted message %d\n", msg);


    k_sleep(5); //give the receiver thread a chance to start

     /* unlock mutex */
    k_mutex_unlock(&my_mutex);
  }
 return;     
}

/* receiver thread for message passing */
void rx_thread( void *dummy1, void *dummy2, void *dummy3)
{
  ARG_UNUSED(dummy1);
  ARG_UNUSED(dummy2);
  ARG_UNUSED(dummy3);

  struct data_item_type data;

  while(count < 101)
  {
    if (k_mutex_lock(&my_mutex, K_FOREVER) != 0){ //lock mutex
         printk("Mutex not locked\n");
    } else {
        k_msgq_get(&my_msgq, &data, K_FOREVER); //receive message
        //printk("Message received %d\n", data.field1);
    }

    k_sleep(5);

     /* unlock mutex */
    k_mutex_unlock(&my_mutex);
  }
 return;
}

/* to initialize interrupt and gpio pins */
void gpio_init()
{

  pinmux = device_get_binding(CONFIG_PINMUX_NAME);

  struct galileo_data *dev = pinmux->driver_data;

  gpiob=dev->gpio_dw;
  if(!gpiob){ 
        printk("error\n");
        return;
   }

   pwm0= dev->pwm0;

  if(!pwm0){
       printk("error\n");
    }

    ret=pinmux_pin_set(pinmux,1,PINMUX_FUNC_A); // IO1 for red led 
  if(ret<0)
      printk("error setting the pin for IO1\n");

      ret=pinmux_pin_set(pinmux,12,PINMUX_FUNC_A); // IO12 for green led  
  if(ret<0)
      printk("error setting the pin for IO12\n");

  ret=pinmux_pin_set(pinmux,3,PINMUX_FUNC_B); // IO3 for input interrupt 
  if(ret<0)
      printk("error setting the pin for IO3\n");

  ret=pinmux_pin_set(pinmux,10,PINMUX_FUNC_A); // IO10 for triggering interrupt 
  if(ret<0)
      printk("error setting the pin for IO10\n");

    ret=pinmux_pin_set(pinmux,2,PINMUX_FUNC_B); // IO2 for input interrupt 
  if(ret<0)
      printk("error setting the pin for IO2\n");

  ret=pinmux_pin_set(pinmux,9,PINMUX_FUNC_C); // IO9 for PWM7
  if(ret<0)
      printk("error setting the pin for IO9\n");

  ret=gpio_pin_configure(gpiob,6,PULL_UP | GPIO_DIR_IN | GPIO_INT | EDGE_RISING); //configure IO3 for interrupt

  ret=gpio_pin_configure(gpiob,5,PULL_UP | GPIO_DIR_IN | GPIO_INT | EDGE_RISING); //configure IO2 for interrupt
}

static int cmd_gpio_int(const struct shell *shell, size_t argc, char **argv)
{
  ARG_UNUSED(argc);
  ARG_UNUSED(argv);
  gpio_pin_write(gpiob, 7, 0);  /* turn green off */
  gpio_pin_write(gpiob, 4, 1);  /* turn red on */
  printk("Running measurement 1: Interrupt latency without background task\n");
  count = 0;
  
  //gpio_init();

  /* configuring interrupt on IO3 */
  gpio_init_callback(&gpio_cb, interrupt_cb, BIT(6));

  ret = gpio_add_callback(gpiob, &gpio_cb);   
  if(ret<0)
      printk("error adding callback\n");

  ret=gpio_pin_enable_callback(gpiob,6);
  if(ret<0) 
      printk("error enabling callback\n");

  while(count<100){
       gpio_pin_write(gpiob, 2, 0); /* triggering interrupt */
       k_sleep(10);
       start_time = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32());
       gpio_pin_write(gpiob, 2, 1);
         
       k_sleep(20);
  }

  gpio_pin_write(gpiob, 2, 0);  /* IO 10 off */
  int avg = 0;
  for(int i = 0; i<100; i++) 
  { 
    avg = avg + measure1_buff[i];
  }

  avg = avg/100;
 
  for(int i = 0; i < 100; i++)
  {
    printk("Interrupt latency with no background task sample %d : %d ns\n", i+1, measure1_buff[i]); 
  } 

  printk("Average Interrupt latency with no background task: %d ns\n", avg);
  gpio_pin_write(gpiob, 4, 0);  /* turn red off */
  gpio_pin_write(gpiob, 7, 1);  /* turn green on */
  
  printk("Finished Measurement 1.\n"); 
  return 0;
} 

static int cmd_gpio_int_back(const struct shell *shell, size_t argc, char **argv)
{
  ARG_UNUSED(argc);
  ARG_UNUSED(argv);
  gpio_pin_write(gpiob, 7, 0);  /* turn green off */
  gpio_pin_write(gpiob, 4, 1);  /* turn red on */
  printk("Running measurement 2: Interrupt latency with background task\n");
  count = 0;
  //gpio_init();

  /* starting measurement 1 */

  /* configure interrupt at IO3 */
  gpio_init_callback(&gpio_cb, interrupt_cb, BIT(6));

  ret = gpio_add_callback(gpiob, &gpio_cb);   
  if(ret<0)
      printk("error adding callback\n");

  ret=gpio_pin_enable_callback(gpiob,6);
  if(ret<0) 
      printk("error enabling callback\n");

  while(count<100){
       gpio_pin_write(gpiob, 2, 0); //trigger IO10 interrupt.
       k_sleep(10);
       start_time = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32());
       gpio_pin_write(gpiob, 2, 1); 
         
       k_sleep(20);
  }

  count = 0;
  gpio_pin_write(gpiob, 2, 0);
  printk("Finished Measurement 1.\n");

  /* starting measurement 2 */

  /* configuring interrupt at IO2 */ 
  gpio_init_callback(&gpio_cb_1, interrupt_cb_1, BIT(5));

  ret = gpio_add_callback(gpiob, &gpio_cb_1);   
  if(ret<0)
      printk("error adding callback\n");

  ret=gpio_pin_enable_callback(gpiob,5);
  if(ret<0) 
      printk("error enabling callback\n");
  
  /* initialize msg q */
  k_msgq_init(&my_msgq, my_msgq_buffer, sizeof(struct data_item_type), 12);

  /* start the sender thread X */
  k_tid_t tid_tx = k_thread_create(&tx_thread_data, tx_thread_stack_area, STACKSIZE, tx_thread, NULL, NULL, NULL, PRIORITY, 0, K_NO_WAIT);
  k_thread_name_set(tid_tx, "rx_thread");

  /* start the receiver thread Y */
  k_tid_t tid_rx = k_thread_create(&rx_thread_data, rx_thread_stack_area, STACKSIZE, rx_thread, NULL, NULL, NULL, PRIORITY, 0, K_NO_WAIT);
  k_thread_name_set(tid_rx, "tx_thread");

  /* start the pwm */
    ret = pwm_pin_set_cycles(pwm0, 7, 0, 1639); /* triggering interrupt using PWM */
  if(ret<0)
      printk("error pwm_pin_set_cycles on PWM7\n");

  while(count < 101) // turn pwm off when done collecting 100 samples.
  {
   printk("");
  }

  /* stop the pwm */
  ret = pwm_pin_set_cycles(pwm0, 7, 100, 0);
  if(ret<0)
     printk("error pwm_pin_set_cycles on PWM7\n");
  
  /* calculating final values */
  int max1=0,max2 = 0;
  int min1=100000000,min2 = 100000000;

  for(int i = 0; i<100; i++)
  {
    measure2_buff[i] = measure2_buff[i+1] - measure2_buff[i];  // calculating delta from measurement 2
  }
  
  for(int i = 0; i < 100 ; i++) // max of measurement 1
  {
    if(measure1_buff[i] > max1)
      {
        max1 = measure1_buff[i];
      }
  }

    for(int i = 0; i < 100 ; i++) // min of measurement 1
  {
    if(measure1_buff[i] < min1)
      {
        min1 = measure1_buff[i];
      }
  }

    for(int i = 0; i < 100 ; i++) // max of delta values from measurement 2
  {
    if(measure2_buff[i] > max2)
      {
        max2 = measure2_buff[i];
      }
  }

    for(int i = 0; i < 100 ; i++) // min of delta values from measurement 2
  {
    if(measure2_buff[i] < min2)
      {
        min2 = measure2_buff[i];
      }
  }

  int avg = 0; //taking average of all delta values 
  for(int i = 0; i<100; i++) 
  { 
    avg = avg + measure2_buff[i];
  }

  avg = avg/100;
    
  int max_final = (max2 - min2 + 2*(max1-min1))/2; //final value calculations 
  

  for(int i = 0; i<100; i++) //printing values 
  {
    printk("Delta sample %d: %d ns\n", i+1, measure2_buff[i]); 
  }

  printk("Average delta value is: %d ns\n", avg);
  printk("The Interrupt latency with a background task is: %d ns\n", max_final);
  gpio_pin_write(gpiob, 4, 0);  /* turn red off */
  gpio_pin_write(gpiob, 7, 1);  /* turn green on */
  printk("Finished Measurement 2.\n");
  
  return 0;
}

static int cmd_context_thread(const struct shell *shell, size_t argc, char **argv)
{ 
  ARG_UNUSED(argc);
  ARG_UNUSED(argv);
  gpio_pin_write(gpiob, 7, 0);  /* turn green off */
  gpio_pin_write(gpiob, 4, 1);  /* turn red on */
  printk("Running Measurement 3: Context switch latency\n");
  count = 0;

  /* create thread A with higher priority */

  tid_a = k_thread_create(&threadA_data,threadA_stack_area,STACKSIZE, threadA, NULL, NULL, NULL, PRIORITY_A, 0, K_NO_WAIT);
  k_thread_name_set(tid_a, "thread_a");

  while(count < 100)
  {
    printk("");
  }

  int avg = 0; // taking average
  for(int i = 0; i<100; i++) 
  { 
    avg = avg + measure3_buff[i];
  }

  avg = avg/100;

   
   for( int i=0; i<100; i++) //printing values 
   {
      printk(" The context switch latency %d : %d ns\n", i, measure3_buff[i]);
   }
   printk("Average context switch latency is %d ns\n",avg);
   gpio_pin_write(gpiob, 4, 0);  /* turn red off */
   gpio_pin_write(gpiob, 7, 1);  /* turn green on */ 
   printk("Finished Measurement 3.\n");     
  return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_measure, 
SHELL_CMD(1, NULL, "Interrupt latency measurement w/o background process.", cmd_gpio_int),
SHELL_CMD(2, NULL, "Interrupt latency measurement w/background process.", cmd_gpio_int_back), 
SHELL_CMD(3, NULL, "Context switch latency measurement.", cmd_context_thread),SHELL_SUBCMD_SET_END);


SHELL_CMD_REGISTER(measure,&sub_measure,"Measure commands",NULL);


void main(void)
{
  printk("Hi! PLease type ""measure"" and press Enter"); 
  gpio_init();
  gpio_pin_write(gpiob, 7, 1); //turn green on
}
