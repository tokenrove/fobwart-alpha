--
-- defobj.lua ($Id$)
-- Default object script
-- Julian Squires <tek@wiw.org> / 2001
--

TRUE=1;
FALSE=0;

function verb_right(o) end
function verb_left(o) end
function verb_up(o) end
function verb_down(o) end
function verb_auto(o) end
function verb_nop(o) end
function verb_act(o, n) end

function verb_talk(o, s)
   talk("<" .. o.name .. "> " .. s)
end

function objectcollide(o, o2)
   if(o2.x < o.x) then
      o.ax = 0;
      o.vx = 0;
      if(o2.ax > 0) then o.ax = o.ax+1; end
   elseif (o2.x > o.x) then
      o.ax = 0;
      o.vx = 0;
      if(o2.ax < 0) then o.ax = o.ax-1; end
   end

   if(o2.y < o.y and o.vy < 0) then
      o.vy = 0;
      o.ay = 0;
   elseif (o2.y > o.y and o.vy > 0) then
      o.vy = 0;
      o.ay = 0;
   end
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
