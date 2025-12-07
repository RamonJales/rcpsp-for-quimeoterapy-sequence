def calcular_atividades(atividades, precedencias, num_pacientes):
    """
    Calcula dinamicamente o número de atividades de tratamento por paciente
    analisando a estrutura do grafo do projeto.
    """
    if num_pacientes == 0:
        return 0

    todos_ids = set(atividades.keys())

    # Encontra os nós de origem (source) - atividades que nunca são sucessoras
    ids_sucessores = set(succ for pred, succ in precedencias)
    source_nodes = todos_ids - ids_sucessores

    # Encontra os nós de término (sink) - atividades que nunca são predecessoras
    ids_predecessores = set(pred for pred, succ in precedencias)
    sink_nodes = todos_ids - ids_predecessores
    
    # O total de atividades "reais" é o total geral menos as estruturais
    total_atividades_reais = len(todos_ids) - len(source_nodes) - len(sink_nodes)

    # Verifica se a divisão é exata
    if total_atividades_reais % num_pacientes == 0:
        return total_atividades_reais // num_pacientes
    else:
        # Retorna None ou lança um erro se a estrutura for inesperada
        print("AVISO: A estrutura do problema não corresponde a um número exato de atividades por paciente.")
        return None

def imprimir_cronograma(cronograma, atividades, precedencias, num_pacientes): # Adicionamos 'precedencias'
    """
    Imprime o cronograma de forma estruturada e visual, calculando dinamicamente
    o número de atividades por paciente.
    """
    # --- Passo 1: Organizar atividades por paciente (AGORA DINÂMICO) ---
    
    # CHAMA A NOVA FUNÇÃO PARA CALCULAR A VARIÁVEL
    atividades_reais_por_paciente = calcular_atividades(atividades, precedencias, num_pacientes)
    
    if atividades_reais_por_paciente is None:
        print("Não foi possível gerar o relatório inteligente devido a uma estrutura de projeto inesperada.")
        return

    cronograma_por_paciente = {i: [] for i in range(1, num_pacientes + 1)}

    for id_ativ, start_time in cronograma.items():
        # A lógica para ignorar a atividade inicial e final continua a mesma
        if id_ativ == 1 or id_ativ > (num_pacientes * atividades_reais_por_paciente + 1):
            continue
        
        # Mapeia o ID da atividade para o número do paciente usando a variável dinâmica
        paciente_num = ((id_ativ - 2) // atividades_reais_por_paciente) + 1
        
        if paciente_num in cronograma_por_paciente:
            duracao = atividades[id_ativ]['duracao']
            end_time = start_time + duracao
            cronograma_por_paciente[paciente_num].append({
                'id': id_ativ,
                'start': start_time,
                'end': end_time,
                'duracao': duracao
            })

    for paciente_num, etapas in sorted(cronograma_por_paciente.items()):
        print(f"\n--- Paciente {paciente_num} ---")
        for etapa in sorted(etapas, key=lambda x: x['start']):
            print(f"  - Atividade {etapa['id']:<3}: Inicia em t={etapa['start']:<3} | Termina em t={etapa['end']:<3} (Duração: {etapa['duracao']})")

    print("="*80)