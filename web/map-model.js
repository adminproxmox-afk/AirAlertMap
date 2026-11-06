(function(root) {
  const api = {
    levels(value,available) {
      return available && typeof value==='string' && /^[NRPY?]{27}$/.test(value)?value:'?'.repeat(27);
    },
    zoom(box,factor,x,y,bounds) {
      const w=Math.max(bounds.w/32,Math.min(bounds.w,box.w/factor)), h=w*bounds.h/bounds.w;
      return api.pan({x:x-(x-box.x)*w/box.w,y:y-(y-box.y)*h/box.h,w,h},0,0,bounds);
    },
    pan(box,dx,dy,bounds) {
      return {...box,x:Math.max(bounds.x,Math.min(bounds.x+bounds.w-box.w,box.x+dx)),
        y:Math.max(bounds.y,Math.min(bounds.y+bounds.h-box.h,box.y+dy))};
    }
  };
  if(typeof module==='object' && module.exports) module.exports=api;
  else root.MapModel=api;
})(typeof window==='object'?window:globalThis);
