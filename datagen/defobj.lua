--
-- defobj.lua ($Id$)
-- Default object script
-- Julian Squires <tek@wiw.org> / 2001
--

TRUE=1;
FALSE=0;
PHYSSCALE=1;

-- Verb values
VERB_NOP=0;
VERB_TALK=1;
VERB_RIGHT=2;
VERB_LEFT=3;
VERB_UP=4;
VERB_DOWN=5;
VERB_ACT=6;
VERB_AUTO=7;
VERB_EXIT=8;

function verb_right(o) end
function verb_left(o) end
function verb_up(o) end
function verb_down(o) end

function verb_auto(o)
    o.ay = 2;
end

function verb_nop(o) end
function verb_act(o, n) end

function verb_talk(o, s)
   talk("<" .. o.name .. "> " .. s)
end

function objectcollide(o, o2)
end

function physics(o) end

function freezevar(n, v)
   local s = "";

   if v == nil then return s end;
   if type(v) == "userdata" or type(v) == "function" then return s end;
   s = n .. "=";
   if type(v) == "string" then s = s .. v;
   elseif type(v) == "table" then
      if v.__visited__ ~= nil then
	 s = s .. v.__visited__;
      else
	 s = s .. "{}\n";
	 v.__visited__ = n;
	 for r,f in v do
	    if r ~= "__visited__" then
	       if type(r) == "string" then
		  s = s .. freezevar(n.."."..r,f);
	       else
		  s = s .. freezevar(n.."["..r.."]",f);
	       end
	    end
	 end
      end
   else s = s .. tostring(v); end
   s = s .. "\n";
   return s;
end


function freezestate(g)
   local s = "";

   for n,v in g do
      s = s .. freezevar(n, v);
   end

   return s;
end

-- EOF defobj.lua
