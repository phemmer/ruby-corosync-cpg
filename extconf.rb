require 'mkmf'

have_library('corosync_common', 'cs_strerror')
have_library('cpg', 'cpg_model_initialize')

create_makefile("CorosyncCPG")

File.open('Makefile','a') do |f|
	f.puts <<EOI

Makefile: #{__FILE__}
	ruby #{__FILE__}

.PHONY: test
test: all
	@echo ----------------------------------------
	sudo ruby test.rb
EOI
end
