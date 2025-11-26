from algorithm import gerar_cronograma


class BoundsCalculator:
    """
    Calcula upper e lower bounds para o makespan
    """
    
    def __init__(self, schedule_scheme):
        self.scheme = schedule_scheme
    
    def compute_upper_bound(self):
        try:
            # Converte schedule scheme para formato do p-SGS
            atividades = self.scheme.activities
            recursos = self.scheme.resources
            precedencias = list(self.scheme.C)
            
            # Incompatibilidades são as disjunções
            incompatibilidades = []
            for (i, j) in self.scheme.D:
                incompatibilidades.append((i, j))
            
            # Executa p-SGS
            cronograma = gerar_cronograma(
                atividades,
                recursos,
                precedencias,
                incompatibilidades
            )
            
            # Calcula makespan
            makespan = 0
            for job_id, start_time in cronograma.items():
                finish_time = start_time + atividades[job_id]['duracao']
                makespan = max(makespan, finish_time)
            
            return makespan
        
        except Exception as e:
            # Se falhar, retorna infinito
            return float('inf')
    
    def compute_lower_bound_simple(self):
        # Calcula o caminho mais longo do source ao sink
        job_ids = sorted(self.scheme.activities.keys())
        
        # Inicializa distâncias
        dist = {job_id: 0 for job_id in job_ids}
        
        # Ordena topologicamente (simplificado)
        # Para cada job em ordem crescente
        for job_id in job_ids:
            duration = self.scheme.activities[job_id]['duracao']
            
            # Para cada sucessor
            for (i, j) in self.scheme.C:
                if i == job_id:
                    dist[j] = max(dist[j], dist[i] + duration)
        
        # Lower bound é a maior distância
        return max(dist.values())
    
    def compute_lower_bound_resource(self):
        lb_resource = 0
        
        for resource_name, capacity in self.scheme.resources.items():
            if capacity == 0:
                continue
            
            total_work = 0
            for job_id, activity in self.scheme.activities.items():
                duration = activity['duracao']
                resource_need = activity['recursos_necessarios'].get(resource_name, 0)
                total_work += duration * resource_need
            
            lb_k = total_work / capacity
            lb_resource = max(lb_resource, lb_k)
        
        return lb_resource
    
    def compute_lower_bound(self):
        lb_critical_path = self.compute_lower_bound_simple()
        lb_resource = self.compute_lower_bound_resource()
        
        return max(lb_critical_path, lb_resource)
    
    def compute_lower_bound_with_heads_tails(self, heads, tails):
        lb = 0
        
        for job_id in self.scheme.activities.keys():
            r_i = heads.get(job_id, 0)
            p_i = self.scheme.activities[job_id]['duracao']
            q_i = tails.get(job_id, 0)
            
            lb = max(lb, r_i + p_i + q_i)
        
        return lb



