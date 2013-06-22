require 'thread'
require 'pry-nav'

puts Process.pid
#binding.pry


def dict_to_hash(dict)
  v = nil
  h = {}
  dict.each_index{|n|
    case n%2
    when 0
      v = dict[n]
    when 1
      k = dict[n]
      h[k] = v
    end
  }
  h
end


$mutex = Mutex.new
$dirs = {}
def make_directory(path)
  $mutex.synchronize{
    unless $dirs[path]
      IO::AFC.connect{|afc|
        afc.mkdir(path)
      }
      $dirs[path] = 0
    end
    $dirs[path] += 1
  }
end

def remove_directory(path)
  $mutex.synchronize{
    $dirs[path] -= 1
    if $dirs[path] == 0
      IO::AFC.connect{|afc|
        afc.unlink(path)
        $dirs.delete(path)
      }
    end
  }
end
