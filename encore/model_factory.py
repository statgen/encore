from .epacts_job_builder import LMEpactsModel, LMMEpactsModel, SkatOEpactsModel, MMSkatOEpactsModel, MMVTEpactsModel, MMSkatEpactsModel, MMCMCEpactsModel
from .saige_job_builder import LinearSaigeModel, BinarySaigeModel
from .savant_job_builder import LinearSavantModel,BinarySavantModel

class ModelFactory:
    __models = []

    @staticmethod
    def list(config = {}):
        def desc(x):
            return {"code": x.model_code, "name": x.model_name, "description": x.model_desc,
                "depends": getattr(x, "depends",  []),
                "filters": getattr(x, "filters",  []),
                "response_class": getattr(x, "response_class", "*")}
        show_models = {x: 1 for x in config.get("AVAIL_MODELS", [])}
        def can_show(x):
            return len(show_models)==0 or x.model_code in show_models
        return [ desc(m) for m in ModelFactory.__models if can_show(m)]

    @staticmethod
    def register(m):
        if not hasattr(m, "model_code"):
            raise ValueError("Missing required attribite 'model_code'")
        ModelFactory.__models.append(m)

    @staticmethod
    def get(model_code, working_directory, config):
        for m in ModelFactory.__models:
            if m.model_code == model_code:
                return m(working_directory, config)
        raise ValueError("Unrecognized model type: {}".format(model_code))

    @staticmethod
    def get_for_model_spec(model_spec, working_directory, config):
        model_code = model_spec.get("type", None)
        if model_code is None:
            raise ValueError("Type not found for model")
        return ModelFactory.get(model_code, working_directory, config)


ModelFactory.register(LMEpactsModel)
ModelFactory.register(LMMEpactsModel)
ModelFactory.register(SkatOEpactsModel)
ModelFactory.register(MMSkatOEpactsModel)
ModelFactory.register(LinearSaigeModel)
ModelFactory.register(BinarySaigeModel)
ModelFactory.register(LinearSavantModel)
ModelFactory.register(BinarySavantModel)
ModelFactory.register(MMSkatEpactsModel)
ModelFactory.register(MMVTEpactsModel)
ModelFactory.register(MMCMCEpactsModel)

            
