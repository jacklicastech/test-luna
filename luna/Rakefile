desc "append file etag and/or release notes to autoupdate index: send release notes from stdin"
task :update_index do
  require 'bundler/setup'
  require 'json'
  require 'aws-sdk'
  etag = Aws::S3::Object.new('appilee', File.join(ENV['PREFIX'], ENV['KEY'])).etag
  etag = etag[1...-1] if etag[/^".*?"$/]
  json = JSON.parse File.read("index.json"), symbolize_names: true

  # find or create the s3 key entry
  key = ENV['KEY'].intern
  json[key] ||= []

  # find or create the change set based on the etag of the s3 object
  change_set = json[key].detect { |set| set[:etag] == etag }
  if change_set.nil?
    change_set = {
      etag: etag,
      release_notes: []
    }
    json[key] << change_set
  end

  # add release notes to the change set, ensuring unique release notes on the
  # off chance there's a duplicate (there shouldn't be)
  change_set[:release_notes].concat $stdin.read.lines.collect { |line| line.chomp }
  change_set[:release_notes].uniq!

  # write the new index
  File.open("index.json", "w") { |f| f.puts JSON.pretty_generate(json) }
end

namespace :install do
  desc 'make'
  task :make do
    raise unless system 'make'
  end

  desc 'make all check'
  task :check do
    raise unless system 'make all check'
  end

  desc 'make, make check, build debian and install debian'
  task :debian do
    Rake::Task['install:check'].invoke
    Rake::Task['debian:amd64'].invoke
    raise unless system 'sudo dpkg -i ./luna-debian-amd64.deb'
  end
end

namespace :debian do
  desc "generate debian amd64 package from compiled binary"
  task :amd64 do
    require 'fileutils'
    include FileUtils

    # strip debug symbols from shared objects and luna binary
    chdir File.dirname(__FILE__)
    mkdir_p 'luna-debian-amd64/usr/bin'
    mkdir_p 'luna-debian-amd64/usr/lib'

    # FIXME instead of getting these files from /usr/local/lib, get them
    # from somewhere more authoritative. Too great a risk of shipping unneeded
    # libraries this way.
    Dir['/usr/local/lib/lib{czmq,zmq,lua,sodium,uriparser}.so.*.*'].each do |fi|
      base = File.basename fi
      symname = base.sub(/\.so\.([^\.]+)\..*/, '.so.\1')
      raise unless system "objcopy --strip-debug --strip-unneeded #{fi} luna-debian-amd64/usr/lib/#{base}"
      chmod 0644, "luna-debian-amd64/usr/lib/#{base}"
      ln_sf base, "luna-debian-amd64/usr/lib/#{symname}"
    end

    # copy in the plugins and language bindings we just built
    rm_rf 'luna-debian-amd64/var/lib'
    mkdir_p 'luna-debian-amd64/var/lib'
    cp_r File.expand_path('../resources', __FILE__), 'luna-debian-amd64/var/lib/luna'
    Dir['.libs/*.so'].each do |fi|
      base = File.basename(fi)
      if base =~ /^liblua_/ then outdir = "luna-debian-amd64/var/lib/luna/bindings"
      else outdir = "luna-debian-amd64/var/lib/luna/plugins"
      end
      raise unless system "objcopy --strip-debug --strip-unneeded #{fi} #{outdir}/#{base}"
      chmod 0644, "#{outdir}/#{base}"
    end

    # manpage
    mkdir_p 'luna-debian-amd64/usr/share/man/man1'
    cp File.expand_path('../README.md', __FILE__), 'luna-debian-amd64/usr/share/man/man1/luna.1'
    if File.exist? 'luna-debian-amd64/usr/share/man/man1/luna.1.gz'
      rm 'luna-debian-amd64/usr/share/man/man1/luna.1.gz'
    end
    raise unless system 'gzip --best luna-debian-amd64/usr/share/man/man1/luna.1'

    # luna binary
    raise unless system 'objcopy --strip-debug --strip-unneeded luna luna-debian-amd64/usr/bin/luna'

    # set file perms
    chmod 0755, 'luna-debian-amd64/DEBIAN/postinst'
    chmod 0644, 'luna-debian-amd64/DEBIAN/shlibs'
    chmod 0755, 'luna-debian-amd64/usr/bin/luna'

    # TODO parse index.json and compile changelog.Debian.gz from it
    touch 'luna-debian-amd64/usr/share/doc/luna/changelog.Debian'
    if File.exist? 'luna-debian-amd64/usr/share/doc/luna/changelog.Debian.gz'
      rm 'luna-debian-amd64/usr/share/doc/luna/changelog.Debian.gz'
    end
    raise unless system 'gzip --best luna-debian-amd64/usr/share/doc/luna/changelog.Debian'

    # build .deb package
    raise unless system 'fakeroot dpkg-deb --build luna-debian-amd64'

    # static analysis of .deb package
    raise unless system 'lintian luna-debian-amd64.deb'
  end
end

desc "generate API documentation for Lua bindings"
task :doc do
  Dir['src/bindings/lua/*.c'].each do |srcfile|
    src = File.read(srcfile)
    state = :none
    documentation = {}
    cur_doc = ''
    cur_sig = ''
    recent = ""
    src.each_char do |c|
      old_state = state
      recent << c
      recent = recent[1..-1] if recent.size > 60
      case state
        when :none
          if c == ?/ then state = :maybe_doc
          else state = :none
          end
        when :maybe_doc
          if c == ?* then state = :documenting
          else state = :none
          end
        when :documenting
          if c == ?* then state = :maybe_nodoc
          end
          cur_doc << c
        when :maybe_nodoc
          if c == ?/ then state = :maybe_sig
          else
            state = :documenting
            cur_doc << c
          end
        when :maybe_sig
          if c == ?\n && cur_sig.strip.size > 0
            cur_sig.strip!
            if cur_sig =~ /^static\s+int\s+(\w+?)_([\w_]+?)\s*\(\s*lua_State\s*\*\s*\w+\)/
              documentation[$1] ||= []
              documentation[$1] << [$2, cur_doc]
            end
            cur_sig = ''
            cur_doc = ''
            state = :none
          else
            cur_sig << c
          end
        else raise "entered unknown state: #{state}"
      end

      # p [ old_state, state, recent ]  if old_state != state
    end

    outfile = File.join(File.expand_path(File.dirname(__FILE__)), "doc", File.basename(srcfile.gsub(/_/, '-'), ".c") + "-lua.md")
    File.open(outfile, "w") do |out|
      documentation.sort { |a,b| a[0] <=> b[0] }.each do |(namespace, funcs)|
        out.puts "## #{namespace}"
        out.puts
        funcs.sort { |a,b| a[0] <=> b[0] }.each do |(name, doc)|
          out.puts "### #{namespace}.#{name}"
          out.puts
          doc.strip.lines.each do |line|
            out.puts line.strip.sub(/^\s*\*\s?/, '')
          end
          out.puts
        end
      end
    end
  end
end
