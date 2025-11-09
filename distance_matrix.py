class DistanceMatrix:
    def __init__(self, schedule_scheme):
        self.scheme = schedule_scheme
        self.n = len(schedule_scheme.activities)
        self.job_ids = sorted(schedule_scheme.activities.keys())
        
        # Mapeia job_id para índice na matriz
        self.id_to_idx = {job_id: idx for idx, job_id in enumerate(self.job_ids)}
        self.idx_to_id = {idx: job_id for job_id, idx in self.id_to_idx.items()}
        
        # Inicializa matriz com infinito
        INF = float('inf')
        self.matrix = [[INF for _ in range(self.n)] for _ in range(self.n)]
        
        # Diagonal é zero
        for i in range(self.n):
            self.matrix[i][i] = 0
        
        # Inicializa com precedências diretas
        self._initialize_from_precedences()
    
    def _initialize_from_precedences(self):
        """
        Inicializa matriz com distâncias diretas das precedências
        d_ij = p_i se i → j ∈ C
        """
        for (i, j) in self.scheme.C:
            idx_i = self.id_to_idx[i]
            idx_j = self.id_to_idx[j]
            duration_i = self.scheme.activities[i]['duracao']
            self.matrix[idx_i][idx_j] = duration_i
    
    def floyd_warshall(self):
        n = self.n
        
        # Floyd-Warshall padrão
        for k in range(n):
            for i in range(n):
                for j in range(n):
                    if self.matrix[i][k] != float('inf') and self.matrix[k][j] != float('inf'):
                        self.matrix[i][j] = min(
                            self.matrix[i][j],
                            self.matrix[i][k] + self.matrix[k][j]
                        )
        
        # Verifica ciclos: se d_ii > 0 para algum i, há ciclo
        for i in range(n):
            if self.matrix[i][i] > 0:
                return False  # Ciclo detectado
        
        return True  # Sem ciclos
    
    def get_distance(self, i, j):
        idx_i = self.id_to_idx[i]
        idx_j = self.id_to_idx[j]
        return self.matrix[idx_i][idx_j]
    
    def update_with_conjunction(self, i, j):
        idx_i = self.id_to_idx[i]
        idx_j = self.id_to_idx[j]
        duration_i = self.scheme.activities[i]['duracao']
        
        # Atualiza distância direta
        self.matrix[idx_i][idx_j] = min(self.matrix[idx_i][idx_j], duration_i)
        
        # Atualiza distâncias transitivas (O(n²))
        n = self.n
        for u in range(n):
            for v in range(n):
                # Caminho passando pela nova aresta i → j
                if self.matrix[u][idx_i] != float('inf') and self.matrix[idx_j][v] != float('inf'):
                    new_dist = self.matrix[u][idx_i] + duration_i + self.matrix[idx_j][v]
                    self.matrix[u][v] = min(self.matrix[u][v], new_dist)
        
        # Verifica ciclos
        for k in range(n):
            if self.matrix[k][k] > 0:
                return False
        
        return True
    
    def can_add_conjunction(self, i, j):
        idx_i = self.id_to_idx[i]
        idx_j = self.id_to_idx[j]
        duration_i = self.scheme.activities[i]['duracao']
        
        # Se já existe caminho de j para i, adicionar i → j cria ciclo
        if self.matrix[idx_j][idx_i] != float('inf'):
            # Verifica se o ciclo seria positivo
            cycle_length = self.matrix[idx_j][idx_i] + duration_i
            if cycle_length > 0:
                return False
        
        return True
    
    def compute_heads_and_tails(self):
        heads = {}
        tails = {}
        
        # Encontra source (job 1) e sink (último job)
        source = 1
        sink = max(self.job_ids)
        
        # r_i = max{r_j + p_j | j → i ∈ C}
        for job_id in self.job_ids:
            if job_id == source:
                heads[job_id] = 0
            else:
                max_head = 0
                predecessors = self.scheme.get_predecessors(job_id)
                for pred in predecessors:
                    pred_duration = self.scheme.activities[pred]['duracao']
                    max_head = max(max_head, heads.get(pred, 0) + pred_duration)
                heads[job_id] = max_head
        for job_id in reversed(self.job_ids):
            if job_id == sink:
                tails[job_id] = 0
            else:
                max_tail = 0
                successors = self.scheme.get_successors(job_id)
                for succ in successors:
                    succ_duration = self.scheme.activities[succ]['duracao']
                    max_tail = max(max_tail, succ_duration + tails.get(succ, 0))
                tails[job_id] = max_tail
        
        return heads, tails
    
    def __str__(self):
        lines = []
        lines.append("Distance Matrix:")
        lines.append("     " + " ".join(f"{self.idx_to_id[j]:4d}" for j in range(min(10, self.n))))
        for i in range(min(10, self.n)):
            row = f"{self.idx_to_id[i]:4d} "
            for j in range(min(10, self.n)):
                val = self.matrix[i][j]
                if val == float('inf'):
                    row += "  ∞ "
                else:
                    row += f"{val:4.0f}"
            lines.append(row)
        return "\n".join(lines)
