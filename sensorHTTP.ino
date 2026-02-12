#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <vector>
#include <string>
#include <Preferences.h>
#include "FS.h"
#include "LittleFS.h"
#include "rgb_lcd.h"
#include "site.h"

using namespace std;

// Configurações do Wi-Fi
const char* ssid = "...";     
const char* password = "..."; 

#define SDA 21
#define SCL 22
#define PINO_RX_DO_ESP 16
#define PINO_TX_DO_ESP 17

// Cria o servidor na porta 80
WebServer server(80);

// --- HARDWARE ---
rgb_lcd lcd;
Preferences preferences;

HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

struct Aluno {
  char nome[80];
  int matricula;           
}; 

vector<Aluno> lista; 
int idAtual = 1; // 

// --- SALVAR NO ARQUIVO ---
void salvarBancoVector() {
  File f = LittleFS.open("/banco_alunos.dat", "w");
   if (f) {
    for (const auto& aluno : lista) {
      f.write((uint8_t*)&aluno, sizeof(Aluno));
    }
    f.close();
    Serial.println("Banco de alunos atualizado na Flash!");
  }
}

// --- CARREGAR DO ARQUIVO ---
void carregarBancoVector() {
  File f = LittleFS.open("/banco_alunos.dat", "r");
  if (f) {
    lista.clear();
    Aluno aluno;
    while (f.read((uint8_t*)&aluno, sizeof(Aluno))) {
      lista.push_back(aluno);
    }
    f.close();
    Serial.println("Banco carregado! Total: " + String(lista.size()));
  }
}

void apagarBancoVector() {
  if(LittleFS.remove("/banco_alunos.dat")) {
    Serial.println("Arquivo apagado com sucesso!");
    lista.clear();
    Serial.println(String(lista.size()));
  } else {
    Serial.println("Falha ao apagar ou arquivo não existia.");
      
  }

  preferences.begin("sistema", false);
  preferences.putInt("proximo_id", 1);
  idAtual = 1;
  preferences.end();

}
// --- CADASTRO ---
void handleCadastro() {
  String nome = server.arg("nome");
  String matricula = server.arg("matricula");

  Serial.println("Recebido do Site:");
  Serial.println("Nome: " + nome);
  Serial.println("Matricula: " + matricula);

  lcd.clear();
  lcd.print("Cadastrando:");
  lcd.setCursor(0, 1);
  lcd.print(nome);

  // Responde rápido para o navegador não dar erro de timeout
  String resposta = "<h1>Recebido!</h1>";
  resposta += "<p>Aluno: " + nome + "</p>";
  resposta += "<p>Matricula: " + matricula + "</p>";
  resposta += "<p><strong>Olhe para o sensor agora!</strong></p>";
  resposta += "<a href='/'>Voltar</a>";
  
  server.send(200, "text/html", resposta);

  int p = -1; 
  Serial.println("Coloque o dedo no sensor");
  delay(1000);

  // Loop 1 - Espera o dedo
  while((p = finger.getImage()) != FINGERPRINT_OK) { 
      // Serial.println("Aguardando..."); 
      delay(10); 
  }
  
  finger.image2Tz(1);
  Serial.println("Dedo 1 capturado");
  
  Serial.println("Tire o dedo");
  lcd.clear(); lcd.print("Tire o dedo");
  
  while (finger.getImage() != FINGERPRINT_NOFINGER) {
    delay(10);
  }

  Serial.println("Coloque o dedo novamente");
  delay(2000);
  
  // Loop 2 - Confirma o dedo
  while ((p = finger.getImage()) != FINGERPRINT_OK) { 
      delay(10);
  }
  
  finger.image2Tz(2);
  Serial.println("Segunda digital capturada");
  
  if (finger.createModel() == FINGERPRINT_OK) {
    
    if (finger.storeModel(idAtual) == FINGERPRINT_OK) {
      
      lcd.clear();
      lcd.print("Salvo ID: "); lcd.print(idAtual);
      Serial.println("Sucesso! ID salvo: " + String(idAtual));
      Serial.println(nome + " cadastrado com sucesso");
      
      Aluno novoAluno;
      strcpy(novoAluno.nome, nome.c_str()); 
      novoAluno.matricula = matricula.toInt();
      
      lista.push_back(novoAluno);
      
      salvarBancoVector();

      // Atualiza ID
      idAtual++;
      
      // CORREÇÃO 2: Usar o mesmo nome "sistema"
      preferences.begin("sistema", false);
      preferences.putInt("proximo_id", idAtual);
      preferences.end();
      
    } else {
      Serial.println("Erro ao salvar no sensor");
      lcd.clear(); lcd.print("Erro Sensor");
    }
  } else {
    lcd.clear(); lcd.print("Erro: Nao Bate");
    Serial.println("Erro: Digitais nao conferem");
  }
  delay(2000);
  lcd.clear(); lcd.print("IP: "); lcd.print(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if(!LittleFS.begin(true)){
    Serial.println("Erro ao montar o LittleFS");
    return;
  }
  
  Wire.begin(SDA, SCL);
  lcd.begin(16, 2);
  lcd.setRGB(255, 255, 0);

  mySerial.begin(57600, SERIAL_8N1, 16, 17); 
  finger.begin(57600);
  
 /*  if (finger.emptyDatabase() == FINGERPRINT_OK) {
    apagarBancoVector();
    Serial.println("Todas as digitais foram apagadas!");

  } else {
    Serial.println("Erro ao apagar ou sensor vazio.");
  } */

  if (finger.verifyPassword()) {
    Serial.println("Sensor de Digital encontrado!");
  } else {
    Serial.println("Sensor de Digital NAO encontrado :(");
  }

  
  preferences.begin("sistema", false);
  idAtual = preferences.getInt("proximo_id", 1); 
  preferences.end();
  Serial.println("Sistema iniciado. Proximo ID a usar: " + String(idAtual));
  

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.clear();
    lcd.print("Conectando Wi-Fi");
  }
  Serial.println("Wi-Fi OK");
  Serial.println(WiFi.localIP()); 
  
  carregarBancoVector();
   
  lcd.clear();
  lcd.print("Wi-Fi OK!");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());
  delay(1000);

  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });

  server.on("/cadastrar", HTTP_POST, handleCadastro);
  
  server.begin();
  Serial.println("Servidor Web Iniciado!");
}

  
int getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    return finger.fingerID;
  }

  if (p == FINGERPRINT_NOTFOUND) return -2;
  return -1;
}

void registrarPresenca(Aluno aluno) {
  // Enviar para o google sheet
}

void registrarOffline(int id) {
  File file = LittleFS.open("/registro_offline.dat", "a");
  Aluno temp;
  strcpy(temp.nome, lista[id-1].nome); 
  temp.matricula = lista[id-1].matricula;
  if(file) {
    f.write((uint8_t*)&temp, sizeof(Aluno));
    file.close();
    Serial.println("Aluno registrado no banco offline");
  }
}

void enviarBancoOffline() {
  FILE file = littleFs.open("/registro-offline.dat","r");
  Aluno aluno;

  if(file) {

    while(f.read((uint8_t*)&aluno, sizeof(Aluno))) {
      registrarPresenca(aluno); // Enviar o registro do aluno para o google Sheet
    }
    if(file.size() == 0) {
      file.remove("/registro-offline.dat");
      Serial.println("Todos os alunos foram registrados");
    } else {
      Serial.prinln("Erro ao registrar todos os alunos");
    }
  } else {
    Serial.println("Registros do banco offline vazio");
  }
}

unsigned long tempoAnterior = 0;
unsigned long intervalo = 3000;
void loop() {
  server.handleClient();
  unsigned long tempoAtual = millis();

  int id = getFingerprintID();

  if(id == -2) {
    Serial.println("ID não cadastrado");
    while (finger.getImage() != FINGERPRINT_NOFINGER) {
      delay(10);
    }
  } else if(id != -1 && id != -2){
    Serial.println("ID encontrado");

    if(WiFi.status() != WL_CONNECTED) {
      registrarOffline(id);
    } else {
      registrarPresenca();
    }

    String nome;
    Serial.println("Nome: ");
    Serial.println(lista[id-1].nome );
    Serial.println("Matrícula: ");
    Serial.println(lista[id-1].matricula);
  }

  if((tempoAtual - tempoAnterior) >= intervalo) {
    tempoAnterior = tempoAtual;

    File file = littleFs.open("/registro-offline.dat","r");
    if(WiFi.status() == WL_CONNECTED && file) {
      enviarBancoOffline();
    }
    
  }

}