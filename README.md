# Como configurar a primeira execução?

##Na primeira execução do sistema, é necessário que seja enviado um arquivo no formato JSON:

```
{
  "network": {
    "wifi": ["SSID", "SENHA"],
    "mqtt": {
      "server": "IP",
      "user": "USUARIO",
      "password": "SENHA",
      "port": "PORTA",
      "topic": "TOPIC"
    }
  },
  "system": {
    "password": "0000"
  }
}
```

