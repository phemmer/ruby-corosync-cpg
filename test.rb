require './CorosyncCPG'

cpg = CorosyncCPG.new()
cpg.join('foobar')
cpg.mcast_joined('hi there')
