#if defined(SWIGPYTHON)
%extend csPen {
        void _Rotate(float a)
        { self->Rotate(a); }
    %pythoncode %{
    def Rotate(self,a):
         return _cspace.csPen__Rotate(a)
    %}
}
#endif

