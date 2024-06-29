local box = {}
box[1] = function(n)
	local fib = box[1]
	if n <= 1 then
		return n
	else
		return fib(n - 1) + fib(n - 2)
	end
end

local fib = box[1]

print(fib(34))
