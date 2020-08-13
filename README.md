# Como configurar a primeira execução?

## Na primeira execução do sistema, é necessário que seja enviado um arquivo no formato JSON:

```
{
  "id_device": "esp8266",
  "network": {
    "wifi": ["SSID", "SENHA"],
    "websocket": {
      "server": "IP",
      "port": "PORTA"
    }
  },
  "system": {
    "password": "0000"
  }
}
```

