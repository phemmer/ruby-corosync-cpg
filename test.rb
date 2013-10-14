require './CorosyncCPG'

cpg = CorosyncCPG.new()
cpg.join('foobar')
cpg.callback_deliver {|msg| puts msg}
cpg.mcast_joined('hi there')
loop do
cpg.dispatch
end
