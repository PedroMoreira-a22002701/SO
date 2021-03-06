#include <stdio.h>
#include "client.h"
#include "main.h"
#include "memory-private.h"
#include "memory.h"
#include "process.h"
#include "proxy.h"
#include "server.h"
#include "sotime.h"
#include <signal.h>

/* Função principal de um Cliente. Deve executar um ciclo infinito
* onde em cada iteração tem dois passos: primeiro, lê uma operação
* da main e se a mesma lida tiver id diferente de -1 e se data->terminate
* ainda for igual a 0, processa-a e escreve-a para os proxies; segundo,
* lê uma resposta dos servidores e se a mesma tiver id diferente de -1 e se
* data->terminate ainda for igual a 0, processa-a. Operações com id igual a
* -1 são ignoradas (op inválida) e se data->terminate for igual a 1 é porque
* foi dada ordem de terminação do programa, portanto deve-se fazer return do
* o número de operações processadas. Para efetuar estes passos, pode usar os
* outros métodos auxiliares definidos em client.h. 
*/
int execute_client(int client_id, struct communication_buffers* buffers, struct main_data* data, struct semaphores* sems){
    struct operation op;
    struct operation *op_ptr = &op;
    signal(SIGINT,SIG_IGN);
    while (1)
    {   
        if (*data->terminate == 1)
        {
            return *data->client_stats;
        }
        
        consume_begin(sems->main_cli);
        client_get_operation(op_ptr, buffers, data, sems);
        consume_end(sems->main_cli);

        if (op_ptr->id != -1 && *data->terminate == 0)
        {
            
            client_process_operation(op_ptr, client_id, data->client_stats);

            produce_begin(sems->cli_prx);
            client_send_operation(op_ptr, buffers, data, sems);
            produce_end(sems->cli_prx);

        }
        
        consume_begin(sems->srv_cli);
        client_receive_answer(op_ptr, buffers, data, sems);
        consume_end(sems->srv_cli);

        if (op_ptr->id != -1 && *data->terminate == 0)
        {
            client_process_answer(op_ptr,data, sems);

        }
        
        if (*data->terminate == 1)
        {
            return *data->client_stats;
        }
    }

    
    
    
}


/* Função que lê uma operação do buffer de memória partilhada entre a 
* main e clientes, efetuando a necessária sincronização antes e depois
* de ler. Quando o processo acorda da sincronização, e antes de tentar ler
* a operação, deve verificar se data->terminate tem valor 1. Em caso 
* afirmativo, retorna imediatamente da função.
*/
void client_get_operation(struct operation* op, struct communication_buffers* buffers, struct main_data* data, struct semaphores* sems){
    if (*data->terminate == 1)
    {
        return;
    }
    else {
        read_rnd_access_buffer(buffers->main_cli, data->buffers_size, op);
        
    }
    
}


/* Função que processa uma operação, alterando o seu campo cliente para o id
* passado como argumento, alterando o estado da mesma para 'C' (client), e 
* incrementando o contador de operações.
*/
void client_process_operation(struct operation* op, int cient_id, int* counter){

    op->client = cient_id;
    op->status = 'C';
    *counter +=1;
    //clock client processou a op
    clock_client_time(op);
}


/* Função que escreve uma operação no buffer de memória partilhada entre
* clientes e proxies, efetuando a necessária sincronização antes e depois
* de escrever.
*/
void client_send_operation(struct operation* op, struct communication_buffers* buffers, struct main_data* data, struct semaphores* sems){
    write_circular_buffer(buffers->cli_prx, data->buffers_size, op);
}


/* Função que lê uma operação do buffer de memória partilhada entre 
* servidores e clientes, efetuando a necessária sincronização antes e
* depois de ler. Quando o processo acorda da sincronização, e antes de
* tentar ler a operação, deve verificar se data->terminate tem valor 1.
* Em caso afirmativo, retorna imediatamente da função.
*/
void client_receive_answer(struct operation* op, struct communication_buffers* buffers, struct main_data* data, struct semaphores* sems){
    if (*data->terminate == 1)
    {
        return;
    }
    
    read_circular_buffer(buffers->srv_cli, data->buffers_size, op);
}


/* Função que guarda uma operação no array de operações finalizadas da
* estrutura data, usando o id da mesma para indexar o array. O acesso à
* estrutura deve ser sincronizada através do semáforo sems->results_mutex.
* Imprime também uma mensagem para o terminal a avisar que a operação 
* terminou.
*/
void client_process_answer(struct operation* op, struct main_data* data, struct semaphores* sems){
    
    semaphore_mutex_lock(sems->results_mutex);
    data->results[op->id] = *op;
    semaphore_mutex_unlock(sems->results_mutex);

    //clock client recebeu op do server
    clock_end_time(op);
    unsigned int before_ns = (op->start_time->tv_sec * 1000000000) + op->start_time->tv_nsec;
    unsigned int after_ns = (op->end_time->tv_sec * 1000000000) + op->end_time->tv_nsec;

    unsigned int tempoTotal = (after_ns - before_ns) / 1.0e6;
    
    /*unsigned int after_client_ns = (op->client_time->tv_sec * 1000000000) + op->client_time->tv_nsec;
    unsigned int after_prxy_ns = (op->proxy_time->tv_sec * 1000000000) + op->proxy_time->tv_nsec;
    unsigned int after_server_ns = (op->server_time->tv_sec * 1000000000) + op->server_time->tv_nsec;*/
    /*unsigned int tempoClient = (after_client_ns - before_ns) / 1.0e6;
    unsigned int tempoProxy = (after_prxy_ns - before_ns) / 1.0e6;
    unsigned int tempoServer = (after_server_ns - before_ns) / 1.0e6;*/
    

    printf("Operation %d is ready to be read! It took %d millisecs\n", op->id,tempoTotal);
    /*printf("Was read by Client %d ms after\n",tempoClient);
    printf("Was read by Proxy %d ms after\n",tempoProxy);
    printf("Was read by Server %d ms after\n",tempoServer);*/

}
