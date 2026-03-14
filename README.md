# ESP32 MQTT Smart Relay (3 Canais)

Firmware para ESP32 que controla **3 relés** via **MQTT**, com **botões físicos**, **persistência de estado**, **configuração Wi-Fi via portal**, **OTA (atualização remota)** e **LED RGB de status**.

Este projeto foi desenvolvido para aplicações de **automação residencial** ou **controle remoto de cargas elétricas** utilizando o protocolo MQTT.

---

# Funcionalidades

- Controle de **3 relés independentes**
- Controle via **MQTT**
- **Botões físicos** para acionamento manual
- **WiFiManager** para configuração de rede sem reprogramação
- **OTA (Over-The-Air)** para atualização de firmware pela rede
- **Memória persistente (NVS)** para manter estado após reinicialização
- **Reconexão automática** de Wi-Fi e MQTT
- **LED RGB de status**
- Arquitetura **FreeRTOS com múltiplas tarefas**
- **Logs de diagnóstico via Serial**

---

# Hardware suportado

- ESP32 Dev Module
- ESP32 com **8MB Flash**

## Pinos utilizados

| Função | GPIO |
|------|------|
| Botão 1 | 26 |
| Botão 2 | 33 |
| Botão 3 | 16 |
| Relé 1 | 19 |
| Relé 2 | 18 |
| Relé 3 | 17 |
| LED R | 14 |
| LED G | 12 |
| LED B | 27 |

---

# Instalação

## 1. Clonar o repositório


## 2. Instalar bibliotecas

Instale no Arduino IDE:

- WiFiManager  
- PubSubClient  
- ArduinoOTA  
- Preferences (já incluída no ESP32 core)

---

## 3. Configurar Arduino IDE

Board:

```
ESP32 Dev Module
```

Flash:

```
8MB
```

Partition Scheme:

```
Default 8MB with OTA
```

---

# Configuração Wi-Fi

Na primeira inicialização o dispositivo cria um Access Point:

```
Configura-Lampadas
```

Conecte-se a ele e abra no navegador:

```
http://192.168.0.0
```

Selecione a rede Wi-Fi e informe a senha.

---

# MQTT

Configure o broker no código:

```cpp
const char* MQTT_SERVER = "192.168.0.000";
```

---

# Tópicos MQTT

## Comando

| Tópico | Função |
|------|------|
| casa/lampada1/set | Controla relé 1 |
| casa/lampada2/set | Controla relé 2 |
| casa/lampada3/set | Controla relé 3 |

### Payload aceito

```
ON
OFF
```

---

## Status

O dispositivo publica o estado:

| Tópico |
|------|
| casa/lampada1/status |
| casa/lampada2/status |
| casa/lampada3/status |

Exemplo:

```
Topic: casa/lampada1/status
Payload: ON
```

---

# Atualização OTA

Depois que o dispositivo estiver conectado na rede, ele aparecerá no Arduino IDE em:

```
Tools → Port → Network Ports
```

Exemplo:

```
esp-lampadas at 192.168.0.000
```

Selecione essa porta e faça **Upload** normalmente.

---

# LED de Status

| Cor | Significado |
|----|----|
| Vermelho | Sem Wi-Fi |
| Azul | Wi-Fi conectado sem MQTT |
| Verde | Sistema online |
| Roxo | Mensagem MQTT recebida |
| Amarelo | Modo configuração WiFiManager |

---

# Arquitetura do firmware

O firmware usa **FreeRTOS** com múltiplas tarefas:

| Task | Função |
|-----|-----|
| Buttons | leitura dos botões |
| MQTTLoop | manutenção da conexão MQTT |
| MQTTProcess | processamento de comandos |
| Reconnect | reconexão Wi-Fi/MQTT |
| Status | monitoramento do sistema |

---

# Segurança

Recomendado para produção:

- adicionar **senha OTA**
- usar **broker MQTT autenticado**
- usar **fonte isolada para ESP32**

---

# Licença

Este projeto é open-source e pode ser usado livremente em projetos pessoais ou comerciais.

---

# Autor

Rodrigo Martins Ribeiro  
Projeto desenvolvido para aplicações de **IoT e automação residencial utilizando ESP32**.
