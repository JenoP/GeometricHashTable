import math, random

TABLE_SIZE=1_000_000
TOL=1e-6
MASK=(1<<64)-1

def mix64(v):
    v=(v+0x9E3779B97F4A7C15)&MASK
    v=((v^(v>>30))*0xBF58476D1CE4E5B9)&MASK
    v=((v^(v>>27))*0x94D049BB133111EB)&MASK
    return v^(v>>31)

def quantize(c):
    return math.floor(c/TOL+0.5) if c>=0 else math.ceil(c/TOL-0.5)

def hash_point(p):
    h=mix64((quantize(p[0])&MASK))
    h=mix64(h^(quantize(p[1])&MASK))
    h=mix64(h^(quantize(p[2])&MASK))
    return h

def hash_triangle(t, order_independent):
    h=[hash_point(p) for p in t]
    if order_independent:
        h.sort()
    key=mix64(h[0])
    key=mix64(key^h[1])
    key=mix64(key^h[2])
    return ((key*TABLE_SIZE)>>64)&0xffffffff

triangles=[]
vertices=[]
with open(r'D:\Code\Repos\GeometricHashTable\GeometricHashTable\wavy_surface_100000.stl','r') as f:
    for line in f:
        parts=line.strip().split()
        if parts and parts[0]=='vertex':
            vertices.append(tuple(map(float,parts[1:])))
            if len(vertices)==3:
                triangles.append(tuple(sorted(vertices)))
                vertices=[]
                if len(triangles)>=16384:
                    break

reference_true={}
reference_false={}
for t in triangles:
    reference_true.setdefault(hash_triangle(t,True),t)
    reference_false.setdefault(hash_triangle(t,False),t)

for count in (64,1024,16384):
    queries=triangles[:count]
    rng=random.Random(0x5EED)
    order=list(range(len(queries)))
    rng.shuffle(order)
    actual=0
    expected=0
    mismatches=0
    for i in range(1_000_000):
        t=queries[order[i%len(order)]]
        rb_true=reference_true.get(hash_triangle(t,True))
        rb_false=reference_false.get(hash_triangle(t,False))
        expected_result=(rb_true is not None and rb_true==t)
        actual_result=(rb_false is not None and rb_false==t)
        expected+=expected_result
        actual+=actual_result
        mismatches+=expected_result!=actual_result
    print(count, 'reference_true=',expected,'table_false=',actual,'mismatches=',mismatches)
