def parse_sm_file(filename):
    """
    Lê um arquivo no formato .sm sem usar imports, extraindo as informações
    para as estruturas de dados necessárias para o algoritmo de agendamento.
    """
    atividades = {}
    precedencias = []
    recursos = {}

    reading_mode = None

    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()

            if not line or line.startswith('*'):
                continue

            if "PRECEDENCE RELATIONS" in line:
                reading_mode = 'precedence'
                continue
            elif "REQUESTS/DURATIONS" in line:
                reading_mode = 'requests'
                continue
            elif "RESOURCEAVAILABILITIES" in line:
                reading_mode = 'resources'
                continue
            
            if line.startswith('-'):
                continue

            if reading_mode == 'precedence':
                # Substituído re.split por line.split()
                parts = line.split()
                job_id = int(parts[0])
                num_successors = int(parts[2])
                if num_successors > 0:
                    successors = [int(s) for s in parts[3:]]
                    for successor in successors:
                        precedencias.append((job_id, successor))

            elif reading_mode == 'requests':
                # Substituído re.split por line.split()
                parts = line.split()
                job_id = int(parts[0])
                duration = int(parts[2])
                reqs = {
                    'R1': int(parts[3]),
                    'R2': int(parts[4]),
                    'R3': int(parts[5]),
                    'R4': int(parts[6]),
                }
                atividades[job_id] = {
                    'id': job_id,
                    'duracao': duration,
                    'recursos_necessarios': reqs
                }

            elif reading_mode == 'resources':
                # Substituído re.split por line.split()
                parts = line.split()
                
                # A verificação de cabeçalho continua funcionando perfeitamente
                if not parts[0].isdigit():
                    continue

                recursos = {
                    'R1': int(parts[0]), # Enfermeiro
                    'R2': int(parts[1]), # Cadeira
                    'R3': int(parts[2]), # Médico
                    'R4': int(parts[3]), # Farmacêutico
                }
                reading_mode = None # Para de ler após encontrar os recursos

    return atividades, precedencias, recursos