def gerar_cronograma(atividades, recursos, precedencias, incompatibilidades):

    # 1 - inicialização
    t = 0
    cronograma = {}

    predecessores_map = {job_id: set() for job_id in atividades}
    for pred, succ in precedencias:
        predecessores_map[succ].add(pred)

    # lógica de fallback para encontrar a atividade inicial
    todos_ids = set(atividades.keys())
    ids_sucessores = set(succ for pred, succ in precedencias)
    atividades_candidatas = list(todos_ids - ids_sucessores)

    if len(atividades_candidatas) == 1:
        id_atividade_inicial = atividades_candidatas[0]
    else:
        print(f"AVISO: Não foi possível determinar uma única atividade inicial pela lógica (candidatas: {len(atividades_candidatas)}). Usando fallback.")
        id_atividade_inicial = min(todos_ids)

    conjunto_completo = {id_atividade_inicial}
    conjunto_ativo = set()
    conjunto_decisao = set(succ for pred, succ in precedencias if pred == id_atividade_inicial)
    cronograma[id_atividade_inicial] = 0

    # 2 - loop principal
    while len(conjunto_completo) < len(atividades):
        atividades_agendadas_nesta_iteracao = set()

        if conjunto_decisao:
            decisao_ordenada = sorted(list(conjunto_decisao))
            for id_j in decisao_ordenada:
                atividade_j = atividades[id_j]
                
                recursos_ok = True
                recursos_usados = {res: 0 for res in recursos.keys()}
                for id_ativo in conjunto_ativo:
                    for res, quantidade in atividades[id_ativo]['recursos_necessarios'].items():
                        recursos_usados[res] += quantidade
                
                for res, qnt_necessaria in atividade_j['recursos_necessarios'].items():
                    if recursos_usados[res] + qnt_necessaria > recursos[res]:
                        recursos_ok = False
                        break
                if not recursos_ok:
                    continue
                
                incompatibilidade_ok = True
                for id_ativo in conjunto_ativo:
                    for par_incompativel in incompatibilidades:
                        if (id_j in par_incompativel) and (id_ativo in par_incompativel):
                            incompatibilidade_ok = False
                            break
                    if not incompatibilidade_ok:
                        break
                if not incompatibilidade_ok:
                    continue

                cronograma[id_j] = t
                conjunto_ativo.add(id_j)
                atividades_agendadas_nesta_iteracao.add(id_j)
            
            conjunto_decisao.difference_update(atividades_agendadas_nesta_iteracao)

        if not conjunto_ativo:
            if len(conjunto_completo) < len(atividades):
                 print(f"AVISO: Impasse de recursos no tempo t={t}. Nenhum trabalho ativo.")
            break
        
        tempo_termino_minimo = 999999
        for id_j in conjunto_ativo:
            tempo_termino_j = cronograma[id_j] + atividades[id_j]['duracao']
            if tempo_termino_j < tempo_termino_minimo:
                tempo_termino_minimo = tempo_termino_j
        
        t = tempo_termino_minimo
        
        atividades_concluidas = set()
        for id_j in list(conjunto_ativo):
            if cronograma[id_j] + atividades[id_j]['duracao'] <= t: 
                atividades_concluidas.add(id_j)
        
        for id_j in atividades_concluidas:
            conjunto_ativo.remove(id_j)
            conjunto_completo.add(id_j)

        ids_nao_agendados = todos_ids - conjunto_completo - conjunto_ativo - conjunto_decisao
        for id_k in ids_nao_agendados:
            if predecessores_map[id_k].issubset(conjunto_completo):
                conjunto_decisao.add(id_k)
                
    return cronograma



def calcular_makespan(cronograma, atividades):
    """Calcula makespan de um cronograma."""
    makespan = 0
    for job_id, start_time in cronograma.items():
        finish_time = start_time + atividades[job_id]['duracao']
        makespan = max(makespan, finish_time)
    return makespan
