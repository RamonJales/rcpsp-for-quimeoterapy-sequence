from distance_matrix import DistanceMatrix

class ImmediateSelection:

    def __init__(self, schedule_scheme):
        self.scheme = schedule_scheme
        self.distance_matrix = None
        self.heads = {}
        self.tails = {}
    
    def apply(self):
        feasible = self.calculate_transitive_distances()
        if not feasible:
            return False, False
        
        success = self.fix_direct_conjunctions()
        
        while True:
            self.calculate_symmetric_triples()
            success_conj = self.fix_direct_conjunctions()
            
            if not success_conj:  # and not success_cliques:
                break
            
            success = True
            
            # Recalcula distâncias após fixar novas relações
            feasible = self.calculate_transitive_distances()
            if not feasible:
                return False, False
        
        return True, success
    
    def calculate_transitive_distances(self):
        self.distance_matrix = DistanceMatrix(self.scheme)
        feasible = self.distance_matrix.floyd_warshall()
        
        if feasible:
            # Calcula heads e tails
            self.heads, self.tails = self.distance_matrix.compute_heads_and_tails()
        
        return feasible
    
    def fix_direct_conjunctions(self):
        success = False
        
        # Para cada disjunção i ~ j ∈ D
        disjunctions_to_remove = []
        for (i, j) in list(self.scheme.D):
            p_i = self.scheme.activities[i]['duracao']
            p_j = self.scheme.activities[j]['duracao']
            
            r_i = self.heads.get(i, 0)
            r_j = self.heads.get(j, 0)
            
            d_ij = self.distance_matrix.get_distance(i, j)
            d_ji = self.distance_matrix.get_distance(j, i)
            
            # Verifica se j não pode preceder i
            if p_i + p_j > d_ij - r_j:
                # Fixa i → j
                self.scheme.add_conjunction(i, j)
                disjunctions_to_remove.append((i, j))
                success = True
            
            # Verifica se i não pode preceder j
            elif p_i + p_j > d_ji - r_i:
                # Fixa j → i
                self.scheme.add_conjunction(j, i)
                disjunctions_to_remove.append((i, j))
                success = True
        
        # Remove disjunções que foram resolvidas
        for disj in disjunctions_to_remove:
            self.scheme.D.discard(disj)
        
        flexibility_to_remove = []
        for (i, j) in list(self.scheme.F):
            p_i = self.scheme.activities[i]['duracao']
            p_j = self.scheme.activities[j]['duracao']
            
            d_ij = self.distance_matrix.get_distance(i, j)
            d_ji = self.distance_matrix.get_distance(j, i)
            
            # Verifica critério de paralelidade
            if d_ij >= -(p_j - 1) and d_ji >= -(p_i - 1):
                self.scheme.add_parallelity(i, j)
                flexibility_to_remove.append((i, j))
                success = True
        
        for flex in flexibility_to_remove:
            self.scheme.F.discard(flex)
        
        return success
    
    def calculate_symmetric_triples(self):
        # Encontra todas as triplas simétricas
        symmetric_triples = []
        
        job_ids = [j for j in self.scheme.activities.keys() 
                   if j != 1 and j != max(self.scheme.activities.keys())]
        
        for i in range(len(job_ids)):
            for j in range(i + 1, len(job_ids)):
                for k in range(j + 1, len(job_ids)):
                    job_i = job_ids[i]
                    job_j = job_ids[j]
                    job_k = job_ids[k]
                    
                    # Verifica se formam tripla simétrica
                    if self._is_parallel(job_i, job_j) and \
                       self._is_parallel(job_i, job_k) and \
                       self._is_parallel(job_j, job_k):
                        symmetric_triples.append((job_i, job_j, job_k))
        
        # Para cada tripla, aplica condições de extensão
        for (i, j, k) in symmetric_triples:
            self._extend_symmetric_triple(i, j, k)
    
    def _is_parallel(self, i, j):
        """Verifica se i||j ∈ N"""
        return (min(i, j), max(i, j)) in self.scheme.N
    
    def _extend_symmetric_triple(self, i, j, k):
        # Para cada atividade l não na tripla
        for l in self.scheme.activities.keys():
            if l in [i, j, k] or l == 1 or l == max(self.scheme.activities.keys()):
                continue
            
            # Condição 1: Se l||i e j,k,l não podem ser paralelos
            # Então l → i pode ser fixado (simplificado)
            if self._is_parallel(l, i):
                # Verifica se l não é paralelo a j e k
                if not self._is_parallel(l, j) and not self._is_parallel(l, k):
                    # Pode adicionar disjunção ou conjunção dependendo do contexto
                    pass
    
    def get_upper_bound_from_heads_tails(self):
        if not self.heads or not self.tails:
            return float('inf')
        
        ub = 0
        for job_id in self.scheme.activities.keys():
            if job_id == 1 or job_id == max(self.scheme.activities.keys()):
                continue
            
            r_i = self.heads.get(job_id, 0)
            p_i = self.scheme.activities[job_id]['duracao']
            q_i = self.tails.get(job_id, 0)
            
            ub = max(ub, r_i + p_i + q_i)
        
        return ub
