a = []

def fib(n):
    fn = a[0]
    if n <= 1:
        return n
    else:
        return fn(n - 1) + fn(n - 2)

a.append(fib)

print(fib(34))
