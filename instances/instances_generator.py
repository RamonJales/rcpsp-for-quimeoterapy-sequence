import os
import re

# --- Configurações Globais ---
BASE_FILENAME = "folfiri_base_multirecurso.sm"
OUTPUT_DIR = "instancias_geradas"


def script1_gerar_variantes_recursos():
    """
    Gera novas instâncias modificando a linha de disponibilidade de recursos.
    Cria cenários com diferentes níveis de restrição (Resource Strength).
    """
    print("--- Executando Script 1: Gerando Variantes de Recursos ---")

    if not os.path.exists(BASE_FILENAME):
        print(f"Erro: Ficheiro base '{BASE_FILENAME}' não encontrado.")
        return

    try:
        with open(BASE_FILENAME, "r") as f:
            base_content_lines = f.readlines()
    except Exception as e:
        print(f"Erro ao ler o ficheiro base: {e}")
        return

    # Definir as configurações de recursos [R1, R2, R3, R4] para testar
    resource_configs = {
        "RS_alto_A": [8, 8, 4, 4],
        "RS_alto_B": [10, 10, 5, 5],
        "RS_medio_A": [5, 6, 2, 2],
        "RS_medio_B": [4, 5, 2, 1],
        "RS_medio_C": [6, 6, 3, 2],
        "RS_baixo_A": [3, 4, 1, 1],
        "RS_baixo_B": [3, 3, 2, 1],
    }

    try:
        # Encontra o índice da linha de cabeçalho
        header_index = base_content_lines.index("RESOURCEAVAILABILITIES:\n")
        # A linha de dados a ser substituída é a próxima
        resource_line_index = header_index + 1
        
        # Garante que o ficheiro base não está corrompido
        if resource_line_index >= len(base_content_lines):
            print("Erro: Ficheiro base parece corrompido. Nenhuma linha de dados encontrada após RESOURCEAVAILABILITIES:")
            return

    except ValueError:
        print("Erro: Palavra-chave 'RESOURCEAVAILABILITIES:' não encontrada no ficheiro base.")
        return

    for name, config in resource_configs.items():
        new_content = base_content_lines[:]
        new_resource_line = f"{config[0]} {config[1]} {config[2]} {config[3]}\n"
        new_content[resource_line_index] = new_resource_line

        output_filename = os.path.join(OUTPUT_DIR, f"folfiri_{name}.sm")
        try:
            with open(output_filename, "w") as f:
                # Escreve o conteúdo até a linha de recursos, garantindo que não há linhas extras
                f.writelines(new_content[:resource_line_index + 1])
            print(f"-> Gerado: {output_filename}")
        except Exception as e:
            print(f"Erro ao escrever o ficheiro '{output_filename}': {e}")

    print("--- Script 1 Concluído ---\n")


def script2_simular_multiplos_pacientes(num_pacientes):
    """
    Gera uma única instância de grande porte simulando a chegada
    simultânea de múltiplos pacientes.
    """
    print(f"--- Executando Script 2: Simulando {num_pacientes} Pacientes ---")

    if not os.path.exists(BASE_FILENAME):
        print(f"Erro: Ficheiro base '{BASE_FILENAME}' não encontrado.")
        return

    with open(BASE_FILENAME, "r") as f:
        content = f.read()

    base_jobs = int(re.search(r"jobs \(incl\. supersource/sink \):\s*(\d+)", content).group(1))
    
    # --- CORREÇÃO PRINCIPAL AQUI ---
    # A regex agora captura a *primeira* e *única* linha de dados após o cabeçalho.
    base_resources_line = re.search(r"RESOURCEAVAILABILITIES:\s*\n(.*)", content, re.DOTALL).group(1).strip()

    precedence_section = re.search(r"PRECEDENCE RELATIONS:\s*\n- jobnr\. mode successors\.\.\.\n(.*?)\n\nREQUESTS/DURATIONS:", content, re.DOTALL).group(1)
    base_precedences = {}
    for line in precedence_section.strip().split('\n'):
        line = line.strip()
        if not line: continue
        parts = list(map(int, line.split()))
        job_id, _, num_succ, *successors = parts
        base_precedences[job_id] = successors

    requests_section = re.search(r"REQUESTS/DURATIONS:\s*\n- jobnr\. mode duration R1 R2 R3 R4\n(.*?)\n\nRESOURCEAVAILABILITIES:", content, re.DOTALL).group(1)
    base_requests = {}
    for line in requests_section.strip().split('\n'):
        line = line.strip()
        if '#' in line: line = line.split('#')[0].strip()
        if not line: continue
        parts = list(map(int, line.split()))
        job_id, _, *req_data = parts
        base_requests[job_id] = req_data

    num_real_activities_base = base_jobs - 2
    new_total_jobs = (num_real_activities_base * num_pacientes) + 2
    new_source_id = 1
    new_sink_id = new_total_jobs

    new_precedences = {i: [] for i in range(1, new_total_jobs + 1)}
    new_requests = {}

    for p in range(num_pacientes):
        for base_job_id in range(2, base_jobs):
            new_job_id = (p * num_real_activities_base) + (base_job_id - 1)
            new_requests[new_job_id] = base_requests[base_job_id]
            if any(base_job_id in succ for job, succ in base_precedences.items() if job == 1):
                new_precedences[new_source_id].append(new_job_id)
            for base_successor_id in base_precedences.get(base_job_id, []):
                if base_successor_id == base_jobs:
                    new_precedences[new_job_id].append(new_sink_id)
                else:
                    new_successor_id = (p * num_real_activities_base) + (base_successor_id - 1)
                    new_precedences[new_job_id].append(new_successor_id)

    new_requests[new_source_id] = [0, 0, 0, 0, 0]
    new_requests[new_sink_id] = [0, 0, 0, 0, 0]

    output_filename = os.path.join(OUTPUT_DIR, f"folfiri_{num_pacientes}_pacientes.sm")
    with open(output_filename, "w") as f:
        f.write("************************************************************************\n")
        f.write(f"* file: {output_filename}\n")
        f.write(f"* - Instance: FOLFIRI protocol for {num_pacientes} patients\n")
        f.write(f"* - nr. of activities:   {new_total_jobs} (incl. source/sink)\n")
        f.write("* - nr. of renewable resources: 4\n")
        f.write("************************************************************************\n")
        f.write(f"jobs (incl. supersource/sink ): {new_total_jobs}\n")
        f.write(f"horizon:                         {200 * num_pacientes} \n")
        f.write("renewable resources:             4\n\n")

        f.write("PRECEDENCE RELATIONS:\n")
        f.write("- jobnr. mode successors...\n")
        for job_id in sorted(new_precedences.keys()):
            successors = sorted(list(set(new_precedences[job_id])))
            f.write(f"  {job_id:<4}1   {len(successors):<2}   {' '.join(map(str, successors))}\n")
        f.write("\n")

        f.write("REQUESTS/DURATIONS:\n")
        f.write("- jobnr. mode duration R1 R2 R3 R4\n")
        for job_id in sorted(new_requests.keys()):
            req_data = new_requests[job_id]
            f.write(f"  {job_id:<4}1   {req_data[0]:<4} {req_data[1]:<2} {req_data[2]:<2} {req_data[3]:<2} {req_data[4]:<2}\n")
        f.write("\n")

        f.write("RESOURCEAVAILABILITIES:\n")
        f.write(f"{base_resources_line}\n")
    
    print(f"-> Gerado: {output_filename} com {new_total_jobs} atividades.")
    print("--- Script 2 Concluído ---")


if __name__ == "__main__":
    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)
    
    # --- Passo Crítico: Antes de executar, corrija o ficheiro base! ---
    # Garanta que 'folfiri_base_multirecurso.sm' tem APENAS UMA linha
    # de dados sob RESOURCEAVAILABILITIES. Ex: "10 10 10 10"

    script1_gerar_variantes_recursos()
    script2_simular_multiplos_pacientes(num_pacientes=5)
    script2_simular_multiplos_pacientes(num_pacientes=25)
    script2_simular_multiplos_pacientes(num_pacientes=40)
    script2_simular_multiplos_pacientes(num_pacientes=50)

