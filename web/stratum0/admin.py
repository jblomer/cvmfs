from django import forms
from django.contrib import admin
from stratum0.models import Stratum1

import cvmfs.repository


class Stratum1AdminForm(forms.ModelForm):
    class Meta:
        model = Stratum1

    def clean(self):
        """
        checks if the provided repository is available and is a replica of a
        locally installed stratum 0 repository
        """
        url  = self.cleaned_data['url']
        fqrn = ""

        try:
            repo = cvmfs.repository.RemoteRepository(url)
            fqrn = repo.fqrn
        except cvmfs.repository.RepositoryNotFound, e:
            raise forms.ValidationError("%s does not point to a CVMFS replica" % url)

        try:
            repo = cvmfs.repository.LocalRepository(fqrn)
        except cvmfs.repository.RepositoryNotFound, e:
            raise forms.ValidationError("the stratum 0 of %s was not found" % fqrn)

        return self.cleaned_data


class Stratum1Admin(admin.ModelAdmin):
    fields       = ['name', 'url']
    form         = Stratum1AdminForm
    list_display = ['url', 'stratum0_fqrn', 'name']
    list_filter  = ['stratum0_fqrn']

    def save_model(self, request, obj, form, change):
        # availability of <obj.url> was checked in Stratum1AdminForm.clean
        repo = cvmfs.repository.RemoteRepository(obj.url)
        obj.stratum0_fqrn = repo.fqrn
        super(Stratum1Admin, self).save_model(request, obj, form, change)


admin.site.register(Stratum1, Stratum1Admin)
