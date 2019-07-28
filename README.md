#######################################################
#           LISTA DE COMANDOS VIA MQTT                #
#######################################################

1 - Comando para adicionar um novo usuário, ou remover,
    no sistema. Caso um ja exista um cartão com o mesmo
    código, será apagado.
    
    add_new_card [<name> | master]
    
    Parâmetros:
    <name> - Nome do novo usuario que será adicionado
    master - Usuário principal. Ele não pode ter um 
             nome diferente. Caso seja adicionado um
             novo usuário com nome "master", será 
             adicionado como principal.
    Obs: Deve ser usado apenas uma das opções
    
    Ex: add_new_card maria
