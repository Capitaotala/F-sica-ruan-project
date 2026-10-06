// =====================================================================
//  Coleta de luminosidade com o sensor TSL2591 (Adafruit) controlada
//  por um botão, com um LED indicando quando a coleta está ativa.
//
//  Funcionamento:
//   - Ao energizar a placa: nada acontece (LED apagado, sem coleta).
//   - 1º toque no botão: o LED acende e o sensor começa a coletar dados.
//   - 2º toque no botão: o LED apaga e o sensor para de coletar.
//   - Os toques seguintes continuam alternando entre ligado e desligado.
//
//  Ligações (iguais ao diagrama do Fritzing em docs/circuito.jpg):
//   - TSL2591 VIN -> 5V | GND -> GND | SCL -> A5 | SDA -> A4
//   - LED: pino 2 -> resistor (220 a 330 ohms) -> perna longa (+) do LED;
//          perna curta (-) do LED -> GND
//   - Botão: um lado -> pino 3 | outro lado -> GND
//            (sem resistor externo: usamos o pull-up interno do Arduino)
//
//  Bibliotecas necessárias (Arduino IDE > Ferramentas > Gerenciar
//  Bibliotecas):
//   - "Adafruit TSL2591 Library"
//   - "Adafruit Unified Sensor" (instalada junto, é uma dependência)
// =====================================================================

#include <Wire.h>              // Inclui a biblioteca de comunicação I2C (usada pelo sensor nos pinos A4/A5)
#include <Adafruit_Sensor.h>   // Inclui a biblioteca base de sensores da Adafruit (exigida pela biblioteca do TSL2591)
#include <Adafruit_TSL2591.h>  // Inclui a biblioteca específica do sensor de luz TSL2591

// ----------------------------- Pinos ---------------------------------
const int PINO_LED   = 2;  // Define que o LED está ligado no pino digital 2 (fio amarelo)
const int PINO_BOTAO = 3;  // Define que o botão está ligado no pino digital 3 (fio laranja)

// -------------------------- Configurações ----------------------------
const unsigned long INTERVALO_LEITURA_MS = 500; // Define o tempo (em milissegundos) entre uma leitura e outra do sensor
const unsigned long TEMPO_DEBOUNCE_MS    = 50;  // Define o tempo para ignorar o "tremido" (bounce) do contato do botão

// ------------------------- Objeto do sensor --------------------------
Adafruit_TSL2591 sensor = Adafruit_TSL2591(2591); // Cria o objeto "sensor" que representa o TSL2591 (2591 é só um número de identificação)

// ------------------------ Variáveis de estado ------------------------
bool coletando = false;                 // Guarda se a coleta está ativa (true) ou parada (false); começa parada
bool sensorOk  = false;                 // Guarda se o sensor foi encontrado corretamente na inicialização
int  leituraAnteriorBotao = HIGH;       // Guarda a última leitura "bruta" do botão (HIGH = solto, por causa do pull-up)
int  estadoEstavelBotao   = HIGH;       // Guarda o estado do botão já filtrado (sem o tremido)
unsigned long instanteMudancaBotao = 0; // Guarda o instante (em ms) em que a leitura do botão mudou pela última vez
unsigned long instanteUltimaLeitura = 0;// Guarda o instante (em ms) da última leitura do sensor
unsigned long instanteInicioColeta  = 0;// Guarda o instante (em ms) em que a coleta atual começou


// =====================================================================
//  setup(): roda UMA vez quando a placa é ligada ou reiniciada
// =====================================================================
void setup() {
  Serial.begin(9600); // Inicia a comunicação com o computador (Monitor Serial) a 9600 bauds

  pinMode(PINO_LED, OUTPUT);         // Configura o pino do LED como SAÍDA (o Arduino vai fornecer tensão a ele)
  digitalWrite(PINO_LED, LOW);       // Garante que o LED comece APAGADO ao energizar a placa
  pinMode(PINO_BOTAO, INPUT_PULLUP); // Configura o pino do botão como ENTRADA com resistor pull-up interno:
                                     // solto = HIGH (5V); apertado = LOW (ligado ao GND pelo botão)

  sensorOk = sensor.begin(); // Tenta encontrar o sensor no barramento I2C; devolve true se ele respondeu
                             // (a biblioteca deixa o sensor em modo de baixo consumo até a primeira leitura)

  if (sensorOk) {                                    // Se o sensor foi encontrado...
    sensor.setGain(TSL2591_GAIN_MED);                // ...ajusta o ganho para médio (25x), bom para luz ambiente
    sensor.setTiming(TSL2591_INTEGRATIONTIME_100MS); // ...ajusta o tempo de exposição de cada leitura para 100 ms
    Serial.println(F("Sensor TSL2591 encontrado. Aperte o botao para iniciar a coleta.")); // ...avisa que está pronto
  } else {                                                          // Se o sensor NÃO foi encontrado...
    Serial.println(F("ERRO: TSL2591 nao encontrado. Verifique VIN, GND, SDA (A4) e SCL (A5).")); // ...mostra o erro
  }
  // Nenhuma coleta é iniciada aqui: o circuito fica parado esperando o botão.
}


// =====================================================================
//  loop(): roda repetidamente, para sempre, depois do setup()
// =====================================================================
void loop() {
  verificarBotao(); // Confere se o botão foi apertado e, se foi, liga/desliga a coleta

  if (coletando && sensorOk) {                                    // Só lê o sensor se a coleta estiver ativa e o sensor estiver OK
    if (millis() - instanteUltimaLeitura >= INTERVALO_LEITURA_MS) { // Confere se já passou o intervalo desde a última leitura
      instanteUltimaLeitura = millis();                            // Marca o instante desta leitura
      lerSensor();                                                 // Faz a leitura e envia os dados para o Monitor Serial
    }
  }
}


// =====================================================================
//  verificarBotao(): detecta um novo aperto do botão (com debounce)
//  e alterna entre "coletando" e "parado"
// =====================================================================
void verificarBotao() {
  int leitura = digitalRead(PINO_BOTAO); // Lê o pino do botão agora (HIGH = solto, LOW = apertado)

  if (leitura != leituraAnteriorBotao) { // Se a leitura mudou em relação à anterior (pode ser tremido do contato)...
    instanteMudancaBotao = millis();     // ...anota o instante da mudança para esperar ela se estabilizar
  }

  if (millis() - instanteMudancaBotao > TEMPO_DEBOUNCE_MS) { // Se a leitura ficou igual por mais que o tempo de debounce...
    if (leitura != estadoEstavelBotao) {                     // ...e ela é diferente do último estado estável...
      estadoEstavelBotao = leitura;                          // ...aceita essa leitura como o novo estado do botão

      if (estadoEstavelBotao == LOW) { // Só age no momento em que o botão é APERTADO (não quando é solto)
        alternarColeta();              // Liga a coleta se estava parada, ou desliga se estava ativa
      }
    }
  }

  leituraAnteriorBotao = leitura; // Guarda a leitura atual para comparar na próxima passagem do loop
}


// =====================================================================
//  alternarColeta(): troca o estado entre coletando e parado
// =====================================================================
void alternarColeta() {
  coletando = !coletando; // Inverte o estado: se era false vira true, se era true vira false

  if (coletando) {                                 // Se a coleta acabou de ser LIGADA...
    digitalWrite(PINO_LED, HIGH);                  // ...acende o LED
    instanteInicioColeta  = millis();              // ...marca o início da coleta (para contar o tempo a partir de zero)
    instanteUltimaLeitura = millis() - INTERVALO_LEITURA_MS; // ...faz a primeira leitura acontecer imediatamente
    Serial.println(F("--- Coleta INICIADA ---"));  // ...avisa no Monitor Serial
    Serial.println(F("tempo_s;lux;luz_total;infravermelho;visivel")); // ...imprime o cabeçalho das colunas de dados
  } else {                                         // Se a coleta acabou de ser DESLIGADA...
    digitalWrite(PINO_LED, LOW);                   // ...apaga o LED
    Serial.println(F("--- Coleta PARADA ---"));    // ...avisa no Monitor Serial
  }
}


// =====================================================================
//  lerSensor(): lê o TSL2591 e imprime uma linha de dados
// =====================================================================
void lerSensor() {
  uint32_t leituraCompleta = sensor.getFullLuminosity(); // Lê os dois canais do sensor de uma vez (leva ~100 ms)
  uint16_t infravermelho = leituraCompleta >> 16;        // Separa os 16 bits de cima: canal infravermelho (IR)
  uint16_t total = leituraCompleta & 0xFFFF;             // Separa os 16 bits de baixo: canal total (visível + IR)
  uint16_t visivel = total - infravermelho;              // Calcula a parte visível: total menos o infravermelho
  float lux = sensor.calculateLux(total, infravermelho); // Converte os valores brutos em lux (unidade de iluminância)

  float tempo = (millis() - instanteInicioColeta) / 1000.0; // Calcula há quantos segundos a coleta começou

  Serial.print(tempo, 2);         // Imprime o tempo em segundos, com 2 casas decimais
  Serial.print(';');              // Imprime o separador de colunas
  Serial.print(lux, 2);           // Imprime a iluminância em lux, com 2 casas decimais
  Serial.print(';');              // Imprime o separador de colunas
  Serial.print(total);            // Imprime o valor bruto do canal total
  Serial.print(';');              // Imprime o separador de colunas
  Serial.print(infravermelho);    // Imprime o valor bruto do canal infravermelho
  Serial.print(';');              // Imprime o separador de colunas
  Serial.println(visivel);        // Imprime o valor bruto da luz visível e pula para a próxima linha
  // Observação: se aparecer lux negativo ou -1, o sensor saturou (luz forte demais
  // para o ganho escolhido). Nesse caso troque TSL2591_GAIN_MED por TSL2591_GAIN_LOW no setup().
}
