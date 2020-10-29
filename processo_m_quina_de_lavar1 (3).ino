//Processo máquina de lavar
#include <LiquidCrystal.h>
#include <Adafruit_NeoPixel.h>

//Macros de set e reset
#define set_bit(reg,bit) 	reg |= (1 << bit)
#define clear_bit(reg,bit) 	reg &= ~(1 << bit)

//Configurações da Comunicação Serial
#define turnOnTransmissor	set_bit(UCSR0B,TXEN0);
#define turnOnReciver 		set_bit(UCSR0B,RXEN0);
#define setIntOnRecive	 	set_bit(UCSR0B,RXCIE0);
#define define8bitPackage	set_bit(UCSR0C,UCSZ00);set_bit(UCSR0C,UCSZ01);
#define setBaudRate			UBRR0 = (f_osc/16)/baud - 1;
#define baud 	9600
#define f_osc 	16000000UL

//Configuração das Interrupções externas
#define risingInt0	set_bit(EICRA,ISC00);set_bit(EICRA,ISC01);
#define interrupt0	set_bit(EIMSK,INT0);
#define DetachInt0	clear_bit(EIMSK,INT0);
#define risingInt1	set_bit(EICRA,ISC10);set_bit(EICRA,ISC11);
#define interrupt1	set_bit(EIMSK,INT1);

//Habilita ponte H (driver do motor)
#define habilita A0

//Saídas para controle do sentido de giro do motor
#define ladoA 10
#define ladoB 11

//Botões de seleção
#define start A1
#define emergencia 2

//Sensores
#define umidade A2
#define tampa_aberta 3

//Pino dos dados NEOPIXEL Etapas e Atuadores
#define PINO_E 8
#define PINO_A 9

//Número de leds na fita das etapas e dos atuadores
#define NUM_ETAPAS 4
#define NUM_ATUADORES 4

//Pinagem dos NEOPIXELS
/*NEOPIXEL - ETAPAS
#define _molho 0
#define _lavar 1
#define _enxague 2
#define _secar 3

//NEOPIXEL - ATUADORES
#define _motor 0
#define _valvula_in 1
#define _valvula_out 2
#define _parado*/

//Inicializa a biblioteca com os números dos pinos da interface LCD
LiquidCrystal lcd(12, 13, 7, 6, 5, 4);

//Configuração com os nº de Leds e o Pino de dados do NEOPIXEL
Adafruit_NeoPixel ETAPAS(NUM_ETAPAS, PINO_E);
Adafruit_NeoPixel ATUADORES(NUM_ATUADORES, PINO_A);

//Etapas do processo
String processo[] = {"molho", "lavar", "enxague", "secar"};

unsigned long tempo_atual;
float val_umidade;
bool selecaoDeLavagem = true;
unsigned int timeMultiplier;
volatile byte estado;

//---------------------Delay-----------------------//
void espera_1s(){
  tempo_atual = millis();
  while(millis() < tempo_atual + 1000) {
  	//Espera 1s
  }
}

void espera_200ms(){
  tempo_atual = millis();
  while(millis() < tempo_atual + 300) {
  	//Espera 300ms
  }
}

//-------------------FunçõesDeLED--------------------//

void acende_LED_ETAPAS(byte n_led){
  ETAPAS.setPixelColor(n_led, 0x0000FF);//Aciona o LED em azul
  ETAPAS.show();//Atualiza os LED
}

void apaga_LED_ETAPAS(byte n_led){
  ETAPAS.setPixelColor(n_led, 0);//Desaciona o LED requerido
  ETAPAS.show();
}

void acende_LED_ATUADORES(byte n_led){
  ATUADORES.setPixelColor(n_led, 0x0000FF);
  ATUADORES.show();
}

void apaga_LED_ATUADORES(byte n_led){
  ATUADORES.setPixelColor(n_led, 0);
  ATUADORES.show();
}

void pronto(){
  //PISCA 3 VEZES a fita ATUADORES - INDICANDO PRONTO
  ATUADORES.clear();
  for(byte j = 0; j < NUM_ATUADORES ; j++){
    for(byte i = 0; i < NUM_ATUADORES; i++) {
      acende_LED_ATUADORES(i);
      espera_200ms();
      apaga_LED_ATUADORES(i);
      delay(10);
     }
  }
}

//-----------------FunçõesDePrint-------------------//

void print(String text,bool break_line=true)
{
  int size = text.length();
  for(int i = 0;i <= size;i++){
    while (!(UCSR0A & ( 1 << UDRE0 )))//verifica se está livre o regitrador de envio
    {/*Espera enquanto não está pronto para enviar*/}
    UDR0 = text[i];
  }
  if(break_line){
  	while (!(UCSR0A & ( 1 << UDRE0 )))//verifica se está livre o regitrador de envio
  	{/*Espera enquanto não está pronto para enviar*/}
  	UDR0 = '\n';
  }
}

void mensagemInicial(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Gostaria de");
  lcd.setCursor(0,1);
  lcd.print("lavar roupa hoje?");
  print("\nGostaria de lavar roupas hoje?");
}

void texto_serial(String etapa, unsigned long duracao_etapa){
    print("Estamos na etapa: ",false);
    print(etapa,false);
    print(", com duracao de ",false);
    print(String(duracao_etapa/1000),false);
    print(" s");
}

void texto_lcd(String etapa, unsigned long duracao_etapa){
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print(etapa);
    lcd.setCursor(10,0);
    lcd.print(duracao_etapa/1000);
    lcd.setCursor(13,0);
    lcd.print(" s");   
}

//-----------------Função Liga motor ---------------//
//Controlar o sentido do giro do motor através da ponte H
//Para girar pro lado A, ladoA=1 e ladoB=0, pro lado B, o inverso
void liga_motor(unsigned long duracao_etapa, byte n){
  digitalWrite(habilita, HIGH);
  tempo_atual = millis();
  while( millis() < tempo_atual + (2*duracao_etapa/n)) {
    acende_LED_ATUADORES(0);
    if( millis() < tempo_atual + (duracao_etapa/n)) {
      digitalWrite(ladoA, HIGH);
      digitalWrite(ladoB, LOW);
    }
    else {
      digitalWrite(ladoB, HIGH);
      digitalWrite(ladoA, LOW);
    }
    delay(50);
  }
  digitalWrite(habilita, LOW);
}
//------------------Configurações-------------------//

void configuraSerial(){
   	setBaudRate;
  	turnOnTransmissor;
	turnOnReciver;
	define8bitPackage;
  	setIntOnRecive;
}

void configuraIntExt0(){
  risingInt0;
  interrupt0;
}

void CancelaInt0(){
  DetachInt0;
}

void configuraIntExt1(){
  risingInt1;
  interrupt1;
}

//------------------Interrupções--------------------//

//Interrompe a função que espera um valor vindo da serial 
ISR(USART_RX_vect){
  timeMultiplier = UDR0-'0';
  if(1 <= timeMultiplier & timeMultiplier<= 3){
	selecaoDeLavagem = false;
  }
}

//Interrompe o ciclo de lavagem quando a tampa está aberta
ISR(INT1_vect){
  while (digitalRead(tampa_aberta)){
  	acende_LED_ATUADORES(3);
  }
  apaga_LED_ATUADORES(3);
}

//Interrompe o ciclo de lavagem quando o botão de emergência é apertado
//e pula para secar apenas
ISR(INT0_vect){
  if (digitalRead(emergencia)){
    ETAPAS.clear();
    ATUADORES.clear();
    estado = 3;
  }
}

//---------------------Setup-----------------------//

void setup()
{
//Configuração dos pinos de entrada e saída digital
  pinMode(start, INPUT);
  pinMode(umidade, INPUT);
  pinMode(tampa_aberta, INPUT);
  pinMode(habilita, OUTPUT);
  pinMode(ladoA, OUTPUT);
  pinMode(ladoB, OUTPUT);
  
// Seta o numero de linhas e colunas no LCD:
  lcd.begin(16, 2);
  
//Inicializa a comunicação com os leds do NEOPIXELS
  ETAPAS.begin();
  ATUADORES.begin();
  
  configuraSerial();
  configuraIntExt1();

//Mensagem de inicialização
  mensagemInicial();
}

//---------------------Loop-----------------------//

void loop()
{ 
//Espera usuário apertar botão start
  if(digitalRead(start)){
    print("Tipo De Lavagem:\n1- Leve\n2- Medio\n3- Pesado");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Tipo De Lavagem:");
    lcd.setCursor(0,1);
    lcd.print("Leve1-2-3Pesada");
    
//Usuário digita pela Serial o tipo de lavagem desejada
    while (selecaoDeLavagem)
    {;/*Espera a resposta*/}

	lcd.clear();
    lcd.print("Inicio processo");
    print("Inicio do processo");
    configuraIntExt0();
    espera_1s();
    estado = 0;
    
    if(estado == 0){
      molho(5000*timeMultiplier);
      apaga_LED_ETAPAS(0);
      espera_1s();
    }
    
    if(estado == 1){
      lavar(6000*timeMultiplier);
      apaga_LED_ETAPAS(1);
      espera_1s();
    }
       
    if(estado == 2){
      enxague(6000*timeMultiplier);
      apaga_LED_ETAPAS(2);
      espera_1s();
    }
      
    if(estado == 3){ 
      secar();
      ETAPAS.clear();
      espera_1s();
    }
    
    lcd.clear();
    lcd.print("FIM DO PROCESSO");
    print("FIM DO PROCESSO");
    ATUADORES.clear();
	espera_1s();
    
//Para reiniciar o processo
    selecaoDeLavagem = true;
    mensagemInicial();
    espera_1s();   
  }
}

//---------------------Etapas-----------------------//

void molho(unsigned long duracao_etapa)
{
  estado = 1;
  acende_LED_ETAPAS(0);
  texto_serial(processo[0],duracao_etapa);
  texto_lcd(processo[0],duracao_etapa);
    
//ENCHE TANQUE
  tempo_atual = millis();
  while(millis() < tempo_atual + (duracao_etapa/2)) {
    acende_LED_ATUADORES(1);
    delay(50);
  }
  apaga_LED_ATUADORES(1);

  liga_motor(duracao_etapa,2);
  apaga_LED_ATUADORES(0);
  
}

void lavar(unsigned long duracao_etapa)
{
  estado = 2;
  acende_LED_ETAPAS(1); 
  texto_serial(processo[1],duracao_etapa);
  texto_lcd(processo[1],duracao_etapa);
    
  liga_motor(duracao_etapa,2);
  apaga_LED_ATUADORES(0);
  
//ESVAZIA TANQUE
  tempo_atual = millis();
  while( millis() < tempo_atual + (duracao_etapa/2)) {
  	acende_LED_ATUADORES(2);
    delay(50);
  }
  apaga_LED_ATUADORES(2);  
}

void enxague(unsigned long duracao_etapa)
{
  estado = 3;
  acende_LED_ETAPAS(2); 
  texto_serial(processo[2],duracao_etapa);
  texto_lcd(processo[2],duracao_etapa);
    
//ENCHE TANQUE 
  tempo_atual = millis();
  while( millis() < tempo_atual + (duracao_etapa/3)) {
	acende_LED_ATUADORES(1);
    delay(50);
  }
  apaga_LED_ATUADORES(1);
 
  liga_motor(duracao_etapa,6);
  apaga_LED_ATUADORES(0);
    
//ESVAZIA TANQUE
  tempo_atual = millis();
  while( millis() < tempo_atual + (duracao_etapa/3)) {
	acende_LED_ATUADORES(2);
    delay(50);
  }
  
}

void secar()
{
  CancelaInt0();
  print("Secando Roupa");
  acende_LED_ETAPAS(3);
  print("Estamos na etapa: ",false);
  print(processo[3]);
  lcd.clear();
  lcd.setCursor(4,0);
  lcd.print(processo[3]);
    
//SECA ROUPA
 
//Converção dos valores da variável analógica para o intervalo de 0 - 100 
  val_umidade = analogRead(umidade)*(100/1023.0); 
  
//Liga o motor até que a umidade esteja baixa
  while(val_umidade >= 15){
    acende_LED_ATUADORES(0);
    //Habilita a ponte H e o giro do motor pro lado A
    digitalWrite(habilita, HIGH);
 	digitalWrite(ladoA, HIGH);
    digitalWrite(ladoB, LOW);
     
    
    val_umidade = analogRead(umidade)*(100/1023.0);
    
  	print("Valor da umidade: ",false);
  	print(String(val_umidade),false);
    print(" %");
    lcd.setCursor(0,1);
    lcd.print("Umidade:");
    lcd.setCursor(9,1);
    lcd.print(val_umidade);
    lcd.setCursor(14,1);
    lcd.print(" %");
    espera_1s();    
  }
  apaga_LED_ATUADORES(2);
  apaga_LED_ATUADORES(0);
  apaga_LED_ETAPAS(3);
  
  //Desabilita ponte H
  digitalWrite(habilita, LOW);
  
  pronto();  
}