

a=["attr",
	"id",
	"style",
	"string",
	"dimen",
	"color",
	"array",
	"drawable",
	"layout",
	"anim",
	"animator",
	"interpolator",
	"mipmap",
	"integer",
	"transition",
	"raw",
	"bool",
	"plurals",
	"menu",
	"font"]


i=0
j=0
a.each do|x|
  print "\t[28 + #{i} * 4] = "
  print j>=256 ? "#{j} & 0xff, #{j} >> 8" : j
  print ", [stringsStart + #{j}] = #{x.size}, #{x.size}"
  print x.split("").map{|x|", '#{x}'"}.join
  puts ", 0, // #{"%#04x" %  (i+1)}"
  i+=1
  j+=3+x.length
end
puts "// total size = #{28+i*4+j}"
