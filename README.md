#######################################################
#           LISTA DE COMANDOS VIA MQTT                #
#######################################################


1 - Usuário
    1.1 - Comando para adicionar um novo usuário, ou 
          no sistema. Caso um ja exista um cartão com
          o mesmo código, será apagado.
    
    add_new_user [<name> | master]?
    
    Parâmetros:
    <name> - Nome do novo usuario que será adicionado
    master - Usuário principal. Ele não pode ter um 
             nome diferente. Caso seja adicionado um
             novo usuário com nome "master", será 
             adicionado como principal.
    Obs: Deve ser usado apenas uma das opções. Após o 
         envio do comando a fechadura vai esperar por
         um novo cartão. Aproxime o cartão que será 
         utilizado. 
    
    Ex: add_new_card maria
    
2 - Rede Wifi
    2.2 - Comando para alterar rede wifi, nome e senha.
    
    change_wifi <ssid>, <password>
    
    Parâmetros:
    <ssid>      - Nome da rede alvo que a fechadura deve 
                  se conectar.
    <password>  - Senha da rede wifi que a fechadura deve
                  se conectar.
    Obs: Ao executar o comando a fechadura vai se 
         desconectar da rede Wifi atual, e tentar se 
         conectar na nova, logo perde a comunicação com 
         o servidor MQtt. Se você estiver distante, pode
         ficar sem comunicação por um certo periodo de 
         tempo. Verifique para não colocar um wifi que
         esteja errado.
